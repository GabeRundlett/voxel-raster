#include "shared.inl"
#include "renderer.hpp"
#include "../player.hpp"
#include <daxa/command_recorder.hpp>
#include <daxa/utils/task_graph_types.hpp>
#include <voxels/voxel_mesh.inl>
#include <voxels/voxel_world.hpp>

#include <daxa/daxa.hpp>
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/imgui.hpp>

#include <imgui_impl_glfw.h>

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

struct RenderChunk {
    daxa::BufferId brick_meshes;
    daxa::BufferId brick_bitmasks;
    daxa::BufferId brick_positions;

    uint32_t brick_count;
};

using Clock = std::chrono::steady_clock;

struct renderer::State {
    daxa::Instance instance;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::ImGuiRenderer imgui_renderer;
    daxa::PipelineManager pipeline_manager;

    std::shared_ptr<daxa::ComputePipeline> allocate_brick_instances_pipeline;
    std::shared_ptr<daxa::ComputePipeline> set_indirect_infos0;
    std::shared_ptr<daxa::ComputePipeline> mesh_voxel_bricks_pipeline;
    std::shared_ptr<daxa::RasterPipeline> draw_visbuffer_pipeline;
    std::shared_ptr<daxa::RasterPipeline> shade_visbuffer_pipeline;
    daxa::TaskImage task_swapchain_image;
    daxa::TaskGraph loop_task_graph;

    daxa::BufferId chunks_buffer;
    daxa::TaskBuffer task_chunks;

    daxa::BufferId brick_meshlet_allocator;
    daxa::BufferId brick_meshlet_metadata;
    daxa::BufferId brick_instance_allocator;

    daxa::TaskBuffer task_brick_meshlet_allocator;
    daxa::TaskBuffer task_brick_meshlet_metadata;
    daxa::TaskBuffer task_brick_instance_allocator;

    std::array<RenderChunk, MAX_CHUNK_COUNT> chunks;

    std::vector<daxa::BufferId> tracked_brick_meshes;
    std::vector<daxa::BufferId> tracked_brick_bitmasks;
    std::vector<daxa::BufferId> tracked_brick_positions;

    daxa::TaskBuffer task_brick_meshes;
    daxa::TaskBuffer task_brick_bitmasks;
    daxa::TaskBuffer task_brick_positions;

    daxa::BufferId brick_visibility_bits;
    daxa::TaskBuffer task_brick_visibility_bits;

    daxa::BufferId visible_brick_indices;
    daxa::TaskBuffer task_visible_brick_indices;

    GpuInput gpu_input;
    Clock::time_point start_time;
};

template <typename T>
inline void allocate_fill_copy(daxa::TaskInterface ti, T value, daxa::TaskBufferAttachmentInfo dst, uint32_t dst_offset = 0) {
    auto alloc = ti.allocator->allocate_fill(value).value();
    ti.recorder.copy_buffer_to_buffer({
        .src_buffer = ti.allocator->buffer(),
        .dst_buffer = dst.ids[0],
        .src_offset = alloc.buffer_offset,
        .dst_offset = dst_offset,
        .size = sizeof(T),
    });
}

template <typename T>
inline void task_fill_buffer(daxa::TaskGraph &tg, daxa::TaskBufferView buffer, T clear_value, uint32_t offset = 0) {
    tg.add_task({
        .attachments = {daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, buffer)},
        .task = [=](daxa::TaskInterface ti) { allocate_fill_copy(ti, clear_value, ti.get(buffer), offset); },
        .name = "fill buffer",
    });
}

template <size_t N>
inline void clear_task_buffers(daxa::TaskGraph &task_graph, std::array<daxa::TaskBufferView, N> const &task_buffer_views, std::array<daxa::BufferClearInfo, N> clear_infos = {}) {
    auto uses = std::vector<daxa::TaskAttachmentInfo>{};
    auto use_count = task_buffer_views.size();
    uses.reserve(use_count);
    for (auto const &task_buffer : task_buffer_views) {
        uses.push_back(daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_buffer));
    }
    task_graph.add_task({
        .attachments = std::move(uses),
        .task = [use_count, clear_infos](daxa::TaskInterface const &ti) {
            for (uint8_t i = 0; i < use_count; ++i) {
                clear_infos[i].buffer = ti.get(daxa::TaskBufferAttachmentIndex{i}).ids[0];
                ti.recorder.clear_buffer(clear_infos[i]);
            }
        },
        .name = "clear buffers",
    });
}
template <size_t N>
inline void clear_task_images(daxa::TaskGraph &task_graph, std::array<daxa::TaskImageView, N> const &task_image_views, std::array<daxa::ClearValue, N> clear_values = {}) {
    auto uses = std::vector<daxa::TaskAttachmentInfo>{};
    auto use_count = task_image_views.size();
    uses.reserve(use_count);
    for (auto const &task_image : task_image_views) {
        uses.push_back(daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, daxa::ImageViewType::REGULAR_2D, task_image));
    }
    task_graph.add_task({
        .attachments = std::move(uses),
        .task = [use_count, clear_values](daxa::TaskInterface const &ti) {
            for (uint8_t i = 0; i < use_count; ++i) {
                ti.recorder.clear_image({
                    .dst_image_layout = ti.get(daxa::TaskImageAttachmentIndex{i}).layout,
                    .clear_value = clear_values[i],
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex{i}).ids[0],
                });
            }
        },
        .name = "clear images",
    });
}

auto round_up_div(auto x, auto y) {
    return (x + y - 1) / y;
}

struct AllocateBrickInstancesTask : AllocateBrickInstancesH::Task {
    AttachmentViews views = {};
    daxa::ComputePipeline *pipeline = {};
    uint32_t *chunk_n;
    void callback(daxa::TaskInterface ti) {
        ti.recorder.set_pipeline(*pipeline);
        AllocateBrickInstancesPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        ti.recorder.push_constant(push);
        ti.recorder.dispatch({*chunk_n, 1, 1});
    }
};

struct SetIndirectInfos0Task : SetIndirectInfos0H::Task {
    AttachmentViews views = {};
    daxa::ComputePipeline *pipeline = {};
    uint32_t *chunk_n;
    void callback(daxa::TaskInterface ti) {
        ti.recorder.set_pipeline(*pipeline);
        SetIndirectInfos0Push push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        ti.recorder.push_constant(push);
        ti.recorder.dispatch({1, 1, 1});
    }
};

struct MeshVoxelBricksTask : MeshVoxelBricksH::Task {
    AttachmentViews views = {};
    daxa::ComputePipeline *pipeline = {};
    uint32_t indirect_offset;
    void callback(daxa::TaskInterface ti) {
        ti.recorder.set_pipeline(*pipeline);
        MeshVoxelBricksPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        ti.recorder.push_constant(push);
        ti.recorder.dispatch_indirect({.indirect_buffer = ti.get(AT.indirect_info).ids[0], .offset = indirect_offset});
    }
};

struct DrawVisbufferTask : DrawVisbufferH::Task {
    AttachmentViews views = {};
    daxa::RasterPipeline *pipeline = {};
    uint32_t indirect_offset;
    void callback(daxa::TaskInterface ti) {
        daxa::TaskImageAttachmentInfo const &image_attach_info = ti.get(AT.render_target);
        daxa::ImageInfo image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
            .color_attachments = std::array{
                daxa::RenderAttachmentInfo{
                    .image_view = image_attach_info.view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<daxa::u32, 4>{0, 0, 0, 0},
                },
            },
            .depth_attachment = daxa::RenderAttachmentInfo{
                .image_view = ti.get(AT.depth_target).view_ids[0],
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue{.depth = 0.0f},
            },
            .render_area = {.width = image_info.size.x, .height = image_info.size.y},
        });
        render_recorder.set_pipeline(*pipeline);
        DrawVisbufferPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        render_recorder.push_constant(push);
        render_recorder.draw_mesh_tasks_indirect({.indirect_buffer = ti.get(AT.indirect_info).ids[0], .offset = indirect_offset, .draw_count = 1, .stride = {}});
        ti.recorder = std::move(render_recorder).end_renderpass();
    }
};

struct ShadeVisbufferTask : ShadeVisbufferH::Task {
    AttachmentViews views = {};
    daxa::RasterPipeline *pipeline = {};
    void callback(daxa::TaskInterface ti) {
        daxa::TaskImageAttachmentInfo const &image_attach_info = ti.get(AT.render_target);
        daxa::ImageInfo image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
            .color_attachments = std::array{
                daxa::RenderAttachmentInfo{
                    .image_view = image_attach_info.view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<daxa::f32, 4>{0.1f, 0.0f, 0.5f, 1.0f},
                },
            },
            .render_area = {.width = image_info.size.x, .height = image_info.size.y},
        });
        render_recorder.set_pipeline(*pipeline);
        ShadeVisbufferPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        render_recorder.push_constant(push);
        render_recorder.draw({.vertex_count = 3});
        ti.recorder = std::move(render_recorder).end_renderpass();
    }
};

struct AnalyzeVisbufferTask : AnalyzeVisbufferH::Task {
    AttachmentViews views = {};
    daxa::ComputePipeline *pipeline = {};
    void callback(daxa::TaskInterface ti) {
        auto const &image_attach_info = ti.get(AT.visbuffer);
        auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        ti.recorder.set_pipeline(*pipeline);
        AnalyzeVisbufferPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        ti.recorder.push_constant(push);
        ti.recorder.dispatch({round_up_div(image_info.size.x, 16), round_up_div(image_info.size.y, 16), 1});
    }
};

void record_tasks(renderer::Renderer self) {
    self->loop_task_graph = daxa::TaskGraph({
        .device = self->device,
        .swapchain = self->swapchain,
        .name = "loop",
    });
    self->loop_task_graph.use_persistent_image(self->task_swapchain_image);
    self->loop_task_graph.use_persistent_buffer(self->task_chunks);
    self->loop_task_graph.use_persistent_buffer(self->task_brick_meshlet_allocator);
    self->loop_task_graph.use_persistent_buffer(self->task_brick_meshlet_metadata);
    self->loop_task_graph.use_persistent_buffer(self->task_brick_instance_allocator);
    self->loop_task_graph.use_persistent_buffer(self->task_brick_meshes);
    self->loop_task_graph.use_persistent_buffer(self->task_brick_bitmasks);
    self->loop_task_graph.use_persistent_buffer(self->task_brick_positions);
    auto task_input_buffer = self->loop_task_graph.create_transient_buffer({
        .size = sizeof(GpuInput),
        .name = "gpu_input",
    });
    self->loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_input_buffer),
        },
        .task = [self, task_input_buffer](daxa::TaskInterface const &ti) {
            allocate_fill_copy(ti, self->gpu_input, ti.get(task_input_buffer));
        },
        .name = "GpuInputUploadTransferTask",
    });
    auto task_visbuffer = self->loop_task_graph.create_transient_image({
        .format = daxa::Format::R32_UINT,
        .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
        .name = "visbuffer",
    });
    auto task_depth = self->loop_task_graph.create_transient_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
        .name = "depth",
    });
#if ENABLE_DEBUG_VIS
    auto task_debug_overdraw = self->loop_task_graph.create_transient_image({
        .format = daxa::Format::R32_UINT,
        .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
        .name = "debug_overdraw",
    });
    clear_task_images(self->loop_task_graph, std::array<daxa::TaskImageView, 1>{task_debug_overdraw}, std::array<daxa::ClearValue, 1>{std::array<uint32_t, 4>{0, 0, 0, 0}});
#endif

    // ResetMeshletAllocatorTask
    task_fill_buffer(self->loop_task_graph, self->task_brick_meshlet_allocator, uint32_t{0});
    task_fill_buffer(self->loop_task_graph, self->task_brick_instance_allocator, uint32_t{0});

    self->loop_task_graph.add_task(AllocateBrickInstancesTask{
        .views = std::array{
            daxa::attachment_view(AllocateBrickInstancesH::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(AllocateBrickInstancesH::AT.chunks, self->task_chunks),
            daxa::attachment_view(AllocateBrickInstancesH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
        },
        .pipeline = self->allocate_brick_instances_pipeline.get(),
        .chunk_n = &self->gpu_input.chunk_n,
    });

    auto task_indirect_infos = self->loop_task_graph.create_transient_buffer({
        .size = sizeof(DispatchIndirectStruct) * 2,
        .name = "indirect_infos",
    });
    self->loop_task_graph.add_task(SetIndirectInfos0Task{
        .views = std::array{
            daxa::attachment_view(SetIndirectInfos0H::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(SetIndirectInfos0H::AT.brick_instance_allocator, self->task_brick_instance_allocator),
            daxa::attachment_view(SetIndirectInfos0H::AT.indirect_infos, task_indirect_infos),
        },
        .pipeline = self->set_indirect_infos0.get(),
    });

    self->loop_task_graph.add_task(MeshVoxelBricksTask{
        .views = std::array{
            daxa::attachment_view(MeshVoxelBricksH::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(MeshVoxelBricksH::AT.chunks, self->task_chunks),
            daxa::attachment_view(MeshVoxelBricksH::AT.bitmasks, self->task_brick_bitmasks),
            daxa::attachment_view(MeshVoxelBricksH::AT.meshes, self->task_brick_meshes),
            daxa::attachment_view(MeshVoxelBricksH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
            daxa::attachment_view(MeshVoxelBricksH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
            daxa::attachment_view(MeshVoxelBricksH::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
            daxa::attachment_view(MeshVoxelBricksH::AT.indirect_info, task_indirect_infos),
        },
        .pipeline = self->mesh_voxel_bricks_pipeline.get(),
        .indirect_offset = sizeof(DispatchIndirectStruct) * 0,
    });
    self->loop_task_graph.add_task(DrawVisbufferTask{
        .views = std::array{
            daxa::attachment_view(DrawVisbufferH::AT.render_target, task_visbuffer),
            daxa::attachment_view(DrawVisbufferH::AT.depth_target, task_depth),
            daxa::attachment_view(DrawVisbufferH::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(DrawVisbufferH::AT.chunks, self->task_chunks),
            daxa::attachment_view(DrawVisbufferH::AT.meshes, self->task_brick_meshes),
            daxa::attachment_view(DrawVisbufferH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
            daxa::attachment_view(DrawVisbufferH::AT.pos_scl, self->task_brick_positions),
            daxa::attachment_view(DrawVisbufferH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
#if ENABLE_DEBUG_VIS
            daxa::attachment_view(DrawVisbufferH::AT.debug_overdraw, task_debug_overdraw),
#endif
            daxa::attachment_view(DrawVisbufferH::AT.indirect_info, task_indirect_infos),
        },
        .pipeline = self->draw_visbuffer_pipeline.get(),
        .indirect_offset = sizeof(DispatchIndirectStruct) * 1,
    });

    self->loop_task_graph.add_task(ShadeVisbufferTask{
        .views = std::array{
            daxa::attachment_view(ShadeVisbufferH::AT.render_target, self->task_swapchain_image),
            daxa::attachment_view(ShadeVisbufferH::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(ShadeVisbufferH::AT.visbuffer, task_visbuffer),
#if ENABLE_DEBUG_VIS
            daxa::attachment_view(ShadeVisbufferH::AT.debug_overdraw, task_debug_overdraw),
#endif
            daxa::attachment_view(ShadeVisbufferH::AT.chunks, self->task_chunks),
            daxa::attachment_view(ShadeVisbufferH::AT.meshes, self->task_brick_meshes),
            daxa::attachment_view(ShadeVisbufferH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
            daxa::attachment_view(ShadeVisbufferH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
            daxa::attachment_view(ShadeVisbufferH::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
        },
        .pipeline = self->shade_visbuffer_pipeline.get(),
    });
    self->loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, daxa::ImageViewType::REGULAR_2D, self->task_swapchain_image),
        },
        .task = [self](daxa::TaskInterface const &ti) {
            auto swapchain_image = self->task_swapchain_image.get_state().images[0];
            self->imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, swapchain_image, self->gpu_input.render_size.x, self->gpu_input.render_size.y);
        },
        .name = "ImGui draw",
    });
    self->loop_task_graph.submit({});
    self->loop_task_graph.present({});
    self->loop_task_graph.complete({});
}

auto get_native_handle(void *glfw_window_ptr) -> daxa::NativeWindowHandle {
#if defined(_WIN32)
    return glfwGetWin32Window((GLFWwindow *)glfw_window_ptr);
#elif defined(__linux__)
    return reinterpret_cast<daxa::NativeWindowHandle>(glfwGetX11Window(glfw_window_ptr));
#endif
}

auto get_native_platform(void * /*unused*/) -> daxa::NativeWindowPlatform {
#if defined(_WIN32)
    return daxa::NativeWindowPlatform::WIN32_API;
#elif defined(__linux__)
    return daxa::NativeWindowPlatform::XLIB_API;
#endif
}

void renderer::init(Renderer &self, void *glfw_window_ptr) {
    self = new State{};

    self->instance = daxa::create_instance({});
    self->device = self->instance.create_device({
        .flags = daxa::DeviceFlags2{
            .mesh_shader_bit = true,
        },
        .name = "my device",
    });

    auto *native_window_handle = get_native_handle(glfw_window_ptr);
    auto native_window_platform = get_native_platform(glfw_window_ptr);
    self->swapchain = self->device.create_swapchain({
        .native_window = native_window_handle,
        .native_window_platform = native_window_platform,
        .surface_format_selector = [](daxa::Format format) {
            switch (format) {
            case daxa::Format::R8G8B8A8_UINT: return 100;
            default: return daxa::default_format_score(format);
            }
        },
        .present_mode = daxa::PresentMode::MAILBOX,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .name = "my swapchain",
    });
    ImGui::CreateContext();
    self->imgui_renderer = daxa::ImGuiRenderer({
        .device = self->device,
        .format = self->swapchain.get_format(),
        .context = ImGui::GetCurrentContext(),
        .use_custom_config = false,
    });
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)glfw_window_ptr, true);
    self->pipeline_manager = daxa::PipelineManager({
        .device = self->device,
        .shader_compile_options = {
            .root_paths = {
                DAXA_SHADER_INCLUDE_DIR,
                "src",
                "src/renderer",
            },
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true,
        },
        .name = "my pipeline manager",
    });

    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"allocate_brick_instances.glsl"}},
            .push_constant_size = sizeof(AllocateBrickInstancesPush),
            .name = "allocate_brick_instances",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->allocate_brick_instances_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"set_indirect_infos.glsl"}},
            .push_constant_size = sizeof(SetIndirectInfos0Push),
            .name = "set_indirect_infos0",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->set_indirect_infos0 = result.value();
    }

    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"mesh_voxel_bricks.glsl"}},
            .push_constant_size = sizeof(MeshVoxelBricksPush),
            .name = "mesh_voxel_bricks",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->mesh_voxel_bricks_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_raster_pipeline({
            .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}, .compile_options = {.required_subgroup_size = 32}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}},
            .task_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}, .compile_options = {.required_subgroup_size = 32}},
            .color_attachments = {{.format = daxa::Format::R32_UINT}},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER,
            },
            .raster = {.polygon_mode = daxa::PolygonMode::FILL},
            .push_constant_size = sizeof(DrawVisbufferPush),
            .name = "draw_visbuffer",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->draw_visbuffer_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"shade_visbuffer.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"shade_visbuffer.glsl"}},
            .color_attachments = {{.format = self->swapchain.get_format()}},
            .raster = {},
            .push_constant_size = sizeof(ShadeVisbufferPush),
            .name = "shade_visbuffer",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->shade_visbuffer_pipeline = result.value();
    }

    self->task_swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};

    self->task_chunks = daxa::TaskBuffer({.name = "task_chunks"});
    self->task_brick_meshlet_allocator = daxa::TaskBuffer({.name = "task_brick_meshlet_allocator"});
    self->task_brick_meshlet_metadata = daxa::TaskBuffer({.name = "task_brick_meshlet_metadata"});
    self->task_brick_instance_allocator = daxa::TaskBuffer({.name = "task_brick_instance_allocator"});
    self->task_brick_visibility_bits = daxa::TaskBuffer({.name = "task_brick_visibility_bits"});
    self->task_visible_brick_indices = daxa::TaskBuffer({.name = "task_visible_brick_indices"});
    self->task_brick_meshes = daxa::TaskBuffer({.name = "task_brick_meshes"});
    self->task_brick_bitmasks = daxa::TaskBuffer({.name = "task_brick_bitmasks"});
    self->task_brick_positions = daxa::TaskBuffer({.name = "task_brick_positions"});

    self->brick_meshlet_allocator = self->device.create_buffer({
        // + 1 for the state at index 0
        .size = sizeof(VoxelMeshlet) * (MAX_MESHLET_COUNT + 1),
        .name = "brick_meshlet_allocator",
    });
    self->brick_meshlet_metadata = self->device.create_buffer({
        // + 1 for the state at index 0
        .size = sizeof(VoxelMeshletMetadata) * (MAX_MESHLET_COUNT + 1),
        .name = "brick_meshlet_metadata",
    });
    self->brick_instance_allocator = self->device.create_buffer({
        // + 1 for the state at index 0
        .size = sizeof(BrickInstance) * (MAX_BRICK_INSTANCE_COUNT + 1),
        .name = "brick_instance_allocator",
    });
    self->task_brick_meshlet_allocator.set_buffers({.buffers = std::array{self->brick_meshlet_allocator}});
    self->task_brick_meshlet_metadata.set_buffers({.buffers = std::array{self->brick_meshlet_metadata}});
    self->task_brick_instance_allocator.set_buffers({.buffers = std::array{self->brick_instance_allocator}});

    self->chunks_buffer = self->device.create_buffer({
        .size = sizeof(VoxelChunk) * 1,
        .name = "chunks_buffer",
    });
    self->task_chunks.set_buffers({.buffers = std::array{self->chunks_buffer}});

    self->brick_visibility_bits = self->device.create_buffer({
        // + 1 for the state at index 0
        .size = sizeof(uint32_t) * MAX_BRICK_INSTANCE_COUNT + 1,
        .name = "brick_visibility_bits",
    });
    self->task_brick_visibility_bits.set_buffers({.buffers = std::array{self->brick_visibility_bits}});

    self->start_time = Clock::now();
}

void renderer::deinit(Renderer self) {
    self->device.wait_idle();
    self->device.collect_garbage();

    if (!self->chunks_buffer.is_empty()) {
        self->device.destroy_buffer(self->chunks_buffer);
    }
    if (!self->brick_meshlet_allocator.is_empty()) {
        self->device.destroy_buffer(self->brick_meshlet_allocator);
    }
    if (!self->brick_meshlet_metadata.is_empty()) {
        self->device.destroy_buffer(self->brick_meshlet_metadata);
    }
    if (!self->brick_instance_allocator.is_empty()) {
        self->device.destroy_buffer(self->brick_instance_allocator);
    }

    if (!self->brick_visibility_bits.is_empty()) {
        self->device.destroy_buffer(self->brick_visibility_bits);
    }
    if (!self->visible_brick_indices.is_empty()) {
        self->device.destroy_buffer(self->visible_brick_indices);
    }

    for (auto &chunk : self->chunks) {
        if (!chunk.brick_meshes.is_empty()) {
            self->device.destroy_buffer(chunk.brick_meshes);
        }
        if (!chunk.brick_bitmasks.is_empty()) {
            self->device.destroy_buffer(chunk.brick_bitmasks);
        }
        if (!chunk.brick_positions.is_empty()) {
            self->device.destroy_buffer(chunk.brick_positions);
        }
    }

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    delete self;
}

void renderer::on_resize(Renderer self, int size_x, int size_y) {
    self->gpu_input.render_size = daxa_u32vec2{uint32_t(size_x), uint32_t(size_y)};
    self->swapchain.resize();

    if (size_x * size_y == 0) {
        return;
    }

    if (!self->visible_brick_indices.is_empty()) {
        self->device.destroy_buffer(self->visible_brick_indices);
    }
    self->visible_brick_indices = self->device.create_buffer({
        // + 1 for the state at index 0
        .size = sizeof(uint32_t) * (size_x * size_y + 1),
        .name = "visible_brick_indices",
    });
    self->task_visible_brick_indices.set_buffers({.buffers = std::array{self->visible_brick_indices}});

    record_tasks(self);
}

void renderer::draw(Renderer self, player::Player player, voxel_world::VoxelWorld voxel_world) {
    auto now = Clock::now();
    auto time = std::chrono::duration<float>(now - self->start_time).count();
    self->gpu_input.time = time;

    auto swapchain_image = self->swapchain.acquire_next_image();
    if (swapchain_image.is_empty()) {
        return;
    }
    auto reload_result = self->pipeline_manager.reload_all();
    if (auto *reload_err = daxa::get_if<daxa::PipelineReloadError>(&reload_result)) {
        std::cout << reload_err->message << std::endl;
    }
    self->task_swapchain_image.set_images({.images = std::span{&swapchain_image, 1}});
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowMetricsWindow(nullptr);
        ImGui::Render();
    }
    player::get_camera(player, &self->gpu_input.cam);

    {
        bool needs_update = false;
        bool has_waited = false;
        bool new_chunks_buffer = false;

        auto new_chunk_n = voxel_world::get_chunk_count(voxel_world);
        if (new_chunk_n != self->gpu_input.chunk_n) {
            has_waited = true;
            new_chunks_buffer = true;
            self->device.wait_idle();
            if (!self->chunks_buffer.is_empty()) {
                self->device.destroy_buffer(self->chunks_buffer);
            }
            self->chunks_buffer = self->device.create_buffer({
                .size = sizeof(VoxelChunk) * new_chunk_n,
                .name = "chunks_buffer",
            });
            self->task_chunks.set_buffers({.buffers = std::array{self->chunks_buffer}});
        }
        self->gpu_input.chunk_n = new_chunk_n;

        auto temp_task_graph = daxa::TaskGraph({
            .device = self->device,
            .name = "temp_task_graph",
        });
        temp_task_graph.use_persistent_buffer(self->task_brick_bitmasks);
        temp_task_graph.use_persistent_buffer(self->task_brick_positions);
        temp_task_graph.use_persistent_buffer(self->task_chunks);

        self->tracked_brick_meshes.clear();
        self->tracked_brick_bitmasks.clear();
        self->tracked_brick_positions.clear();

        for (uint32_t chunk_i = 0; chunk_i < self->gpu_input.chunk_n; ++chunk_i) {
            auto &chunk = self->chunks[chunk_i];
            chunk.brick_count = voxel_world::get_voxel_brick_count(voxel_world, chunk_i);

            auto needs_write_chunk = new_chunks_buffer;

            if (voxel_world::chunk_bricks_changed(voxel_world, chunk_i)) {
                needs_write_chunk = true;
                VoxelBrickBitmask *bitmasks = voxel_world::get_voxel_brick_bitmasks(voxel_world, chunk_i);
                int *positions = voxel_world::get_voxel_brick_positions(voxel_world, chunk_i);

                if (!has_waited) {
                    self->device.wait_idle();
                    has_waited = true;
                }

                if (!chunk.brick_meshes.is_empty()) {
                    self->device.destroy_buffer(chunk.brick_meshes);
                }
                if (!chunk.brick_bitmasks.is_empty()) {
                    self->device.destroy_buffer(chunk.brick_bitmasks);
                }
                if (!chunk.brick_positions.is_empty()) {
                    self->device.destroy_buffer(chunk.brick_positions);
                }

                if (chunk.brick_count == 0) {
                    continue;
                }

                chunk.brick_meshes = self->device.create_buffer({
                    .size = sizeof(VoxelBrickMesh) * chunk.brick_count,
                    .name = "brick_meshes",
                });
                chunk.brick_bitmasks = self->device.create_buffer({
                    .size = sizeof(VoxelBrickBitmask) * chunk.brick_count,
                    .name = "brick_bitmasks",
                });
                chunk.brick_positions = self->device.create_buffer({
                    .size = sizeof(daxa_i32vec4) * chunk.brick_count,
                    .name = "brick_positions",
                });

                // The buffer will be at the current size of tracked buffers.
                auto buffer_view_index = self->tracked_brick_meshes.size();

                temp_task_graph.add_task({
                    .attachments = {
                        daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, self->task_brick_bitmasks),
                        daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, self->task_brick_positions),
                    },
                    .task = [&, bitmasks, positions, buffer_view_index](daxa::TaskInterface const &ti) {
                        auto upload = [&ti, buffer_view_index](daxa::TaskBufferView buffer_view, void *data, uint64_t size) {
                            auto staging_input_buffer = ti.device.create_buffer({
                                .size = size,
                                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                                .name = "staging_input_buffer",
                            });
                            ti.recorder.destroy_buffer_deferred(staging_input_buffer);
                            auto *buffer_ptr = ti.device.get_host_address(staging_input_buffer).value();
                            memcpy(buffer_ptr, data, size);
                            ti.recorder.copy_buffer_to_buffer({
                                .src_buffer = staging_input_buffer,
                                .dst_buffer = ti.get(buffer_view).ids[buffer_view_index],
                                .size = size,
                            });
                        };

                        upload(self->task_brick_bitmasks, bitmasks, sizeof(VoxelBrickBitmask) * chunk.brick_count);
                        upload(self->task_brick_positions, positions, sizeof(daxa_i32vec4) * chunk.brick_count);
                    },
                    .name = "upload bricks",
                });

                needs_update = true;
            }

            if (chunk.brick_count > 0) {
                // put buffers into tracked buffer list
                self->tracked_brick_meshes.push_back(chunk.brick_meshes);
                self->tracked_brick_bitmasks.push_back(chunk.brick_bitmasks);
                self->tracked_brick_positions.push_back(chunk.brick_positions);
            }

            if (needs_write_chunk) {
                auto voxel_chunk = VoxelChunk{
                    .bitmasks = self->device.get_device_address(chunk.brick_bitmasks).value(),
                    .meshes = self->device.get_device_address(chunk.brick_meshes).value(),
                    .pos_scl = self->device.get_device_address(chunk.brick_positions).value(),
                    .brick_n = chunk.brick_count,
                };
                voxel_world::get_chunk_pos(voxel_world, chunk_i, &voxel_chunk.pos.x);
                task_fill_buffer(temp_task_graph, self->task_chunks, voxel_chunk, sizeof(VoxelChunk) * chunk_i);
            }
        }

        if (has_waited) {
            // Theoretically only needs to happen if `has_waited` is true, because that's when buffers are recreated.
            self->task_brick_meshes.set_buffers({.buffers = self->tracked_brick_meshes});
            self->task_brick_bitmasks.set_buffers({.buffers = self->tracked_brick_bitmasks});
            self->task_brick_positions.set_buffers({.buffers = self->tracked_brick_positions});
        }

        if (needs_update) {
            temp_task_graph.submit({});
            temp_task_graph.complete({});
            temp_task_graph.execute({});
        }
    }

    self->loop_task_graph.execute({});
    self->device.collect_garbage();
}
