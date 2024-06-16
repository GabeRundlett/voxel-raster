#include "shared.inl"
#include "renderer.hpp"
#include "../player.hpp"
#include <daxa/command_recorder.hpp>
#include <daxa/gpu_resources.hpp>
#include <daxa/types.hpp>
#include <daxa/utils/task_graph_types.hpp>
#include <imgui.h>
#include <voxels/voxel_mesh.inl>
#include <voxels/voxel_world.hpp>

#include <daxa/daxa.hpp>
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/imgui.hpp>
#include <daxa/utils/fsr2.hpp>

#include <imgui_impl_glfw.h>

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

struct renderer::ChunkState {
    daxa::BufferId brick_data;
    uint32_t brick_count;
    uint32_t tracked_index = 0xffffffff;
    std::vector<VoxelBrickBitmask> bitmasks;
    std::vector<VoxelAttribBrick> attribs;
    std::vector<int> positions;
    bool needs_update = false;
};

using Clock = std::chrono::steady_clock;

struct renderer::State {
    daxa::Instance instance;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::ImGuiRenderer imgui_renderer;
    daxa::PipelineManager pipeline_manager;
    daxa::Fsr2Context fsr2_context;

    std::shared_ptr<daxa::ComputePipeline> clear_draw_flags_pipeline;
    std::shared_ptr<daxa::ComputePipeline> allocate_brick_instances_pipeline;
    std::shared_ptr<daxa::ComputePipeline> set_indirect_infos0;
    std::shared_ptr<daxa::ComputePipeline> set_indirect_infos1;
    std::shared_ptr<daxa::ComputePipeline> set_indirect_infos2;
    std::shared_ptr<daxa::ComputePipeline> mesh_voxel_bricks_pipeline;
    std::shared_ptr<daxa::RasterPipeline> draw_visbuffer_pipeline;
    std::shared_ptr<daxa::RasterPipeline> draw_visbuffer_depth_cull_pipeline;
    std::shared_ptr<daxa::RasterPipeline> draw_visbuffer_observer_pipeline;
    std::shared_ptr<daxa::RasterPipeline> draw_visbuffer_observer_depth_cull_pipeline;
    std::shared_ptr<daxa::ComputePipeline> compute_rasterize_pipeline;
    std::shared_ptr<daxa::ComputePipeline> compute_rasterize_depth_cull_pipeline;
    std::shared_ptr<daxa::ComputePipeline> compute_rasterize_observer_pipeline;
    std::shared_ptr<daxa::ComputePipeline> compute_rasterize_observer_depth_cull_pipeline;
    std::shared_ptr<daxa::RasterPipeline> post_processing_pipeline;
    std::shared_ptr<daxa::ComputePipeline> shade_visbuffer_pipeline;
    std::shared_ptr<daxa::ComputePipeline> analyze_visbuffer_pipeline;
    std::shared_ptr<daxa::ComputePipeline> gen_hiz_pipeline;
    std::shared_ptr<daxa::RasterPipeline> debug_lines_pipeline;
    std::shared_ptr<daxa::RasterPipeline> debug_points_pipeline;
    daxa::TaskImage task_swapchain_image;
    daxa::TaskGraph loop_task_graph;
    daxa::TaskGraph loop_empty_task_graph;

    std::vector<Chunk> chunks_to_update;
    std::vector<VoxelChunk> chunks;
    daxa::BufferId chunks_buffer;
    daxa::TaskBuffer task_chunks;

    daxa::BufferId brick_meshlet_allocator;
    daxa::BufferId brick_meshlet_metadata;
    daxa::BufferId brick_instance_allocator;

    daxa::TaskBuffer task_brick_meshlet_allocator;
    daxa::TaskBuffer task_brick_meshlet_metadata;
    daxa::TaskBuffer task_brick_instance_allocator;

    std::vector<daxa::BufferId> tracked_brick_data;
    daxa::TaskBuffer task_brick_data;

    daxa::BufferId brick_visibility_bits;
    daxa::TaskBuffer task_brick_visibility_bits;

    daxa::BufferId visible_brick_instance_allocator;
    daxa::TaskBuffer task_visible_brick_instance_allocator;

    GpuInput gpu_input{};
    Clock::time_point start_time;
    Clock::time_point prev_time;

    std::vector<std::array<daxa_f32vec3, 3>> debug_lines;
    std::vector<std::array<daxa_f32vec3, 3>> debug_points;

    bool use_fsr2 = false;
    bool draw_from_observer = false;
    bool needs_record = false;
    bool needs_update = false;
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
        .task = [use_count, clear_infos](daxa::TaskInterface const &ti) mutable {
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

inline constexpr auto round_up_div(auto x, auto y) {
    return (x + y - 1) / y;
}

inline constexpr auto find_msb(uint32_t v) -> uint32_t {
    uint32_t index = 0;
    while (v != 0) {
        v = v >> 1;
        index = index + 1;
    }
    return index;
}
inline constexpr auto find_next_lower_po2(uint32_t v) -> uint32_t {
    auto const msb = find_msb(v);
    return 1u << ((msb == 0 ? 1 : msb) - 1);
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

struct SetIndirectInfosTask : SetIndirectInfosH::Task {
    AttachmentViews views = {};
    daxa::ComputePipeline *pipeline = {};
    uint32_t *chunk_n;
    void callback(daxa::TaskInterface ti) {
        ti.recorder.set_pipeline(*pipeline);
        SetIndirectInfosPush push = {};
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
    bool first = false;
    void callback(daxa::TaskInterface ti) {
        auto const &image_attach_info = ti.get(AT.visbuffer64);
        auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        auto render_recorder = std::move(ti.recorder).begin_renderpass({
            // .color_attachments = std::array{
            //     daxa::RenderAttachmentInfo{
            //         .image_view = image_attach_info.view_ids[0],
            //         .load_op = first ? daxa::AttachmentLoadOp::CLEAR : daxa::AttachmentLoadOp::LOAD,
            //         .clear_value = std::array<daxa::u32, 4>{0, 0, 0, 0},
            //     },
            // },
            // .depth_attachment = daxa::RenderAttachmentInfo{
            //     .image_view = ti.get(AT.depth_target).view_ids[0],
            //     .load_op = first ? daxa::AttachmentLoadOp::CLEAR : daxa::AttachmentLoadOp::LOAD,
            //     .clear_value = daxa::DepthValue{.depth = 0.0f},
            // },
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

struct ComputeRasterizeTask : ComputeRasterizeH::Task {
    AttachmentViews views = {};
    daxa::ComputePipeline *pipeline = {};
    uint32_t indirect_offset;
    void callback(daxa::TaskInterface ti) {
        ti.recorder.set_pipeline(*pipeline);
        ComputeRasterizePush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        ti.recorder.push_constant(push);
        ti.recorder.dispatch_indirect({.indirect_buffer = ti.get(AT.indirect_info).ids[0], .offset = indirect_offset});
    }
};

struct ShadeVisbufferTask : ShadeVisbufferH::Task {
    AttachmentViews views = {};
    daxa::ComputePipeline *pipeline = {};
    void callback(daxa::TaskInterface ti) {
        auto const &image_attach_info = ti.get(AT.color);
        auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        ti.recorder.set_pipeline(*pipeline);
        ShadeVisbufferPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        ti.recorder.push_constant(push);
        ti.recorder.dispatch({round_up_div(image_info.size.x, 16), round_up_div(image_info.size.y, 16), 1});
    }
};

struct PostProcessingTask : PostProcessingH::Task {
    AttachmentViews views = {};
    daxa::RasterPipeline *pipeline = {};
    void callback(daxa::TaskInterface ti) {
        auto const &image_attach_info = ti.get(AT.render_target);
        auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        auto render_recorder = std::move(ti.recorder).begin_renderpass({
            .color_attachments = std::array{
                daxa::RenderAttachmentInfo{
                    .image_view = image_attach_info.view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                },
            },
            .render_area = {.width = image_info.size.x, .height = image_info.size.y},
        });
        render_recorder.set_pipeline(*pipeline);
        PostProcessingPush push = {};
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
        auto const &image_attach_info = ti.get(AT.visbuffer64);
        auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        ti.recorder.set_pipeline(*pipeline);
        AnalyzeVisbufferPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        ti.recorder.push_constant(push);
        ti.recorder.dispatch({round_up_div(image_info.size.x, 16), round_up_div(image_info.size.y, 16), 1});
    }
};

struct GenHizTask : GenHizH::Task {
    AttachmentViews views = {};
    daxa::ComputePipeline *pipeline = {};
    void callback(daxa::TaskInterface ti) {
        ti.recorder.set_pipeline(*pipeline);
        auto const &image_attach_info = ti.get(AT.src);
        auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        auto const dispatch_x = round_up_div(find_next_lower_po2(image_info.size.x) * 2, GEN_HIZ_WINDOW_X);
        auto const dispatch_y = round_up_div(find_next_lower_po2(image_info.size.y) * 2, GEN_HIZ_WINDOW_Y);
        GenHizPush push = {
            .counter = ti.allocator->allocate_fill(0u).value().device_address,
            .mip_count = ti.get(AT.mips).view.slice.level_count,
            .total_workgroup_count = dispatch_x * dispatch_y,
        };
        ti.assign_attachment_shader_blob(push.uses.value);
        ti.recorder.push_constant(push);
        ti.recorder.dispatch({dispatch_x, dispatch_y, 1});
    }
};

struct ClearDrawFlagsTask : ClearDrawFlagsH::Task {
    AttachmentViews views = {};
    daxa::ComputePipeline *pipeline = {};
    uint32_t indirect_offset;
    void callback(daxa::TaskInterface ti) {
        ti.recorder.set_pipeline(*pipeline);
        ClearDrawFlagsPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        ti.recorder.push_constant(push);
        ti.recorder.dispatch_indirect({.indirect_buffer = ti.get(AT.indirect_info).ids[0], .offset = indirect_offset});
    }
};

struct DebugLinesTask : DebugLinesH::Task {
    AttachmentViews views = {};
    daxa::RasterPipeline *pipeline = {};
    std::vector<std::array<daxa_f32vec3, 3>> *debug_lines = {};

    void callback(daxa::TaskInterface ti) {
        auto const &image_attach_info = ti.get(AT.render_target);
        auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        auto render_recorder = std::move(ti.recorder).begin_renderpass({
            .color_attachments = std::array{
                daxa::RenderAttachmentInfo{
                    .image_view = image_attach_info.view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                },
            },
            .render_area = {.width = image_info.size.x, .height = image_info.size.y},
        });
        render_recorder.set_pipeline(*pipeline);
        DebugLinesPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);

        auto size = debug_lines->size() * sizeof(std::array<daxa_f32vec3, 3>);
        auto alloc = ti.allocator->allocate(size).value();
        memcpy(alloc.host_address, debug_lines->data(), size);
        push.vertex_data = alloc.device_address;

        render_recorder.push_constant(push);
        render_recorder.draw({.vertex_count = uint32_t(2 * debug_lines->size())});
        ti.recorder = std::move(render_recorder).end_renderpass();
    }
};

struct DebugPointsTask : DebugLinesH::Task {
    AttachmentViews views = {};
    daxa::RasterPipeline *pipeline = {};
    std::vector<std::array<daxa_f32vec3, 3>> *debug_points = {};

    void callback(daxa::TaskInterface ti) {
        auto const &image_attach_info = ti.get(AT.render_target);
        auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        auto render_recorder = std::move(ti.recorder).begin_renderpass({
            .color_attachments = std::array{
                daxa::RenderAttachmentInfo{
                    .image_view = image_attach_info.view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                },
            },
            .render_area = {.width = image_info.size.x, .height = image_info.size.y},
        });
        render_recorder.set_pipeline(*pipeline);
        DebugLinesPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);

        auto size = debug_points->size() * sizeof(std::array<daxa_f32vec3, 3>);
        auto alloc = ti.allocator->allocate(size).value();
        memcpy(alloc.host_address, debug_points->data(), size);
        push.vertex_data = alloc.device_address;

        render_recorder.push_constant(push);
        render_recorder.draw({.vertex_count = uint32_t(6 * debug_points->size())});
        ti.recorder = std::move(render_recorder).end_renderpass();
    }
};

auto task_gen_hiz_single_pass(renderer::Renderer self, daxa::TaskGraph &task_graph, daxa::TaskBufferView gpu_input, daxa::TaskImageView task_visbuffer64) -> daxa::TaskImageView {
    auto const x_ = self->gpu_input.next_lower_po2_render_size.x;
    auto const y_ = self->gpu_input.next_lower_po2_render_size.y;
    auto mip_count = static_cast<uint32_t>(std::ceil(std::log2(std::max(x_, y_))));
    mip_count = std::min(mip_count, uint32_t(GEN_HIZ_LEVELS_PER_DISPATCH));
    auto task_hiz = task_graph.create_transient_image({
        .format = daxa::Format::R32_SFLOAT,
        .size = {x_, y_, 1},
        .mip_level_count = mip_count,
        .array_layer_count = 1,
        .sample_count = 1,
        .name = "hiz",
    });
    task_graph.add_task(GenHizTask{
        .views = std::array{
            daxa::attachment_view(GenHizH::AT.gpu_input, gpu_input),
            daxa::attachment_view(GenHizH::AT.src, task_visbuffer64),
            daxa::attachment_view(GenHizH::AT.mips, task_hiz),
        },
        .pipeline = self->gen_hiz_pipeline.get(),
    });
    return task_hiz;
}

void record_tasks(renderer::Renderer self) {
    {
        self->loop_empty_task_graph = daxa::TaskGraph({
            .device = self->device,
            .swapchain = self->swapchain,
            .name = "loop_empty",
        });
        self->loop_empty_task_graph.use_persistent_image(self->task_swapchain_image);

        clear_task_images(self->loop_empty_task_graph, std::array<daxa::TaskImageView, 1>{self->task_swapchain_image}, std::array<daxa::ClearValue, 1>{std::array<uint32_t, 4>{0, 0, 0, 0}});

        self->loop_empty_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, daxa::ImageViewType::REGULAR_2D, self->task_swapchain_image),
            },
            .task = [self](daxa::TaskInterface const &ti) {
                auto swapchain_image = self->task_swapchain_image.get_state().images[0];
                self->imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, swapchain_image, self->gpu_input.render_size.x, self->gpu_input.render_size.y);
            },
            .name = "ImGui draw",
        });

        self->loop_empty_task_graph.submit({});
        self->loop_empty_task_graph.present({});
        self->loop_empty_task_graph.complete({});
    }

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
    self->loop_task_graph.use_persistent_buffer(self->task_brick_data);
    self->loop_task_graph.use_persistent_buffer(self->task_brick_visibility_bits);
    self->loop_task_graph.use_persistent_buffer(self->task_visible_brick_instance_allocator);
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
#if ENABLE_DEBUG_VIS
    auto task_debug_overdraw = self->loop_task_graph.create_transient_image({
        .format = daxa::Format::R32_UINT,
        .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
        .name = "debug_overdraw",
    });
    clear_task_images(self->loop_task_graph, std::array<daxa::TaskImageView, 1>{task_debug_overdraw}, std::array<daxa::ClearValue, 1>{std::array<uint32_t, 4>{0, 0, 0, 0}});
#endif

    auto task_indirect_infos = self->loop_task_graph.create_transient_buffer({
        .size = sizeof(DispatchIndirectStruct) * 4,
        .name = "indirect_infos",
    });

    auto task_visbuffer64 = self->loop_task_graph.create_transient_image({
        .format = daxa::Format::R64_UINT,
        .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
        .name = "visbuffer64",
    });
    clear_task_images(self->loop_task_graph, std::array<daxa::TaskImageView, 1>{task_visbuffer64}, std::array<daxa::ClearValue, 1>{std::array<uint32_t, 4>{0, 0, 0, 0}});

    auto task_observer_visbuffer64 = daxa::TaskImageView{};
    if (self->draw_from_observer) {
        task_observer_visbuffer64 = self->loop_task_graph.create_transient_image({
            .format = daxa::Format::R64_UINT,
            .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
            .name = "observer_visbuffer64",
        });
        clear_task_images(self->loop_task_graph, std::array<daxa::TaskImageView, 1>{task_observer_visbuffer64}, std::array<daxa::ClearValue, 1>{std::array<uint32_t, 4>{0, 0, 0, 0}});
    }

    auto draw_brick_instances = [&, first_draw = true](daxa::TaskBufferView task_brick_instance_allocator, daxa::TaskImageView hiz) mutable {
        if (first_draw) {
            task_fill_buffer(self->loop_task_graph, self->task_brick_meshlet_allocator, daxa_u32vec2{});
        }

        self->loop_task_graph.add_task(SetIndirectInfosTask{
            .views = std::array{
                daxa::attachment_view(SetIndirectInfosH::AT.gpu_input, task_input_buffer),
                daxa::attachment_view(SetIndirectInfosH::AT.brick_instance_allocator, task_brick_instance_allocator),
                daxa::attachment_view(SetIndirectInfosH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(SetIndirectInfosH::AT.indirect_info, task_indirect_infos),
            },
            .pipeline = first_draw ? self->set_indirect_infos0.get() : self->set_indirect_infos1.get(),
        });

        self->loop_task_graph.add_task(MeshVoxelBricksTask{
            .views = std::array{
                daxa::attachment_view(MeshVoxelBricksH::AT.gpu_input, task_input_buffer),
                daxa::attachment_view(MeshVoxelBricksH::AT.chunks, self->task_chunks),
                daxa::attachment_view(MeshVoxelBricksH::AT.brick_data, self->task_brick_data),
                daxa::attachment_view(MeshVoxelBricksH::AT.brick_instance_allocator, task_brick_instance_allocator),
                daxa::attachment_view(MeshVoxelBricksH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(MeshVoxelBricksH::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
                daxa::attachment_view(MeshVoxelBricksH::AT.indirect_info, task_indirect_infos),
            },
            .pipeline = self->mesh_voxel_bricks_pipeline.get(),
            .indirect_offset = sizeof(DispatchIndirectStruct) * 0,
        });

        self->loop_task_graph.add_task(SetIndirectInfosTask{
            .views = std::array{
                daxa::attachment_view(SetIndirectInfosH::AT.gpu_input, task_input_buffer),
                daxa::attachment_view(SetIndirectInfosH::AT.brick_instance_allocator, task_brick_instance_allocator),
                daxa::attachment_view(SetIndirectInfosH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(SetIndirectInfosH::AT.indirect_info, task_indirect_infos),
            },
            .pipeline = self->set_indirect_infos2.get(),
        });

        self->loop_task_graph.add_task(DrawVisbufferTask{
            .views = std::array{
                daxa::attachment_view(DrawVisbufferH::AT.visbuffer64, task_visbuffer64),
                daxa::attachment_view(DrawVisbufferH::AT.gpu_input, task_input_buffer),
                daxa::attachment_view(DrawVisbufferH::AT.chunks, self->task_chunks),
                daxa::attachment_view(DrawVisbufferH::AT.brick_data, self->task_brick_data),
                daxa::attachment_view(DrawVisbufferH::AT.brick_instance_allocator, task_brick_instance_allocator),
                daxa::attachment_view(DrawVisbufferH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(DrawVisbufferH::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
#if ENABLE_DEBUG_VIS
                daxa::attachment_view(DrawVisbufferH::AT.debug_overdraw, task_debug_overdraw),
#endif
                daxa::attachment_view(DrawVisbufferH::AT.indirect_info, task_indirect_infos),
                daxa::attachment_view(DrawVisbufferH::AT.hiz, hiz),
            },
            .pipeline = hiz == daxa::NullTaskImage ? self->draw_visbuffer_pipeline.get() : self->draw_visbuffer_depth_cull_pipeline.get(),
            .indirect_offset = sizeof(DispatchIndirectStruct) * 3,
            .first = first_draw,
        });

        self->loop_task_graph.add_task(ComputeRasterizeTask{
            .views = std::array{
                daxa::attachment_view(ComputeRasterizeH::AT.visbuffer64, task_visbuffer64),
                daxa::attachment_view(ComputeRasterizeH::AT.gpu_input, task_input_buffer),
                daxa::attachment_view(ComputeRasterizeH::AT.chunks, self->task_chunks),
                daxa::attachment_view(ComputeRasterizeH::AT.brick_data, self->task_brick_data),
                daxa::attachment_view(ComputeRasterizeH::AT.brick_instance_allocator, task_brick_instance_allocator),
                daxa::attachment_view(ComputeRasterizeH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(ComputeRasterizeH::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
                daxa::attachment_view(ComputeRasterizeH::AT.indirect_info, task_indirect_infos),
#if ENABLE_DEBUG_VIS
                daxa::attachment_view(ComputeRasterizeH::AT.debug_overdraw, task_debug_overdraw),
#endif
                daxa::attachment_view(ComputeRasterizeH::AT.hiz, hiz),
            },
            .pipeline = hiz == daxa::NullTaskImage ? self->compute_rasterize_pipeline.get() : self->compute_rasterize_depth_cull_pipeline.get(),
            .indirect_offset = sizeof(DispatchIndirectStruct) * 1,
        });

        if (self->draw_from_observer) {
            self->loop_task_graph.add_task(DrawVisbufferTask{
                .views = std::array{
                    daxa::attachment_view(DrawVisbufferH::AT.visbuffer64, task_observer_visbuffer64),
                    daxa::attachment_view(DrawVisbufferH::AT.gpu_input, task_input_buffer),
                    daxa::attachment_view(DrawVisbufferH::AT.chunks, self->task_chunks),
                    daxa::attachment_view(DrawVisbufferH::AT.brick_data, self->task_brick_data),
                    daxa::attachment_view(DrawVisbufferH::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(DrawVisbufferH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(DrawVisbufferH::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
#if ENABLE_DEBUG_VIS
                    daxa::attachment_view(DrawVisbufferH::AT.debug_overdraw, task_debug_overdraw),
#endif
                    daxa::attachment_view(DrawVisbufferH::AT.indirect_info, task_indirect_infos),
                    daxa::attachment_view(DrawVisbufferH::AT.hiz, hiz),
                },
                .pipeline = hiz == daxa::NullTaskImage ? self->draw_visbuffer_observer_pipeline.get() : self->draw_visbuffer_observer_depth_cull_pipeline.get(),
                .indirect_offset = sizeof(DispatchIndirectStruct) * 3,
                .first = first_draw,
            });

            self->loop_task_graph.add_task(ComputeRasterizeTask{
                .views = std::array{
                    daxa::attachment_view(ComputeRasterizeH::AT.visbuffer64, task_observer_visbuffer64),
                    daxa::attachment_view(ComputeRasterizeH::AT.gpu_input, task_input_buffer),
                    daxa::attachment_view(ComputeRasterizeH::AT.chunks, self->task_chunks),
                    daxa::attachment_view(ComputeRasterizeH::AT.brick_data, self->task_brick_data),
                    daxa::attachment_view(ComputeRasterizeH::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(ComputeRasterizeH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(ComputeRasterizeH::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
                    daxa::attachment_view(ComputeRasterizeH::AT.indirect_info, task_indirect_infos),
#if ENABLE_DEBUG_VIS
                    daxa::attachment_view(ComputeRasterizeH::AT.debug_overdraw, task_debug_overdraw),
#endif
                    daxa::attachment_view(ComputeRasterizeH::AT.hiz, hiz),
                },
                .pipeline = hiz == daxa::NullTaskImage ? self->compute_rasterize_observer_pipeline.get() : self->compute_rasterize_observer_depth_cull_pipeline.get(),
                .indirect_offset = sizeof(DispatchIndirectStruct) * 1,
            });
        }

        first_draw = false;
    };

    // draw previously visible bricks
    draw_brick_instances(self->task_brick_instance_allocator, daxa::NullTaskImage);

    // build hi-z
    auto hiz = task_gen_hiz_single_pass(self, self->loop_task_graph, task_input_buffer, task_visbuffer64);

    // cull and draw the rest
    self->loop_task_graph.add_task(AllocateBrickInstancesTask{
        .views = std::array{
            daxa::attachment_view(AllocateBrickInstancesH::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(AllocateBrickInstancesH::AT.chunks, self->task_chunks),
            daxa::attachment_view(AllocateBrickInstancesH::AT.brick_data, self->task_brick_data),
            daxa::attachment_view(AllocateBrickInstancesH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
            daxa::attachment_view(AllocateBrickInstancesH::AT.hiz, hiz),
        },
        .pipeline = self->allocate_brick_instances_pipeline.get(),
        .chunk_n = &self->gpu_input.chunk_n,
    });
    draw_brick_instances(self->task_brick_instance_allocator, hiz);

    // clear draw flags. This needs to be done before the Analyze visbuffer pass,
    // since AnalyzeVisbuffer populates them again.
    self->loop_task_graph.add_task(SetIndirectInfosTask{
        .views = std::array{
            daxa::attachment_view(SetIndirectInfosH::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(SetIndirectInfosH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
            daxa::attachment_view(SetIndirectInfosH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
            daxa::attachment_view(SetIndirectInfosH::AT.indirect_info, task_indirect_infos),
        },
        .pipeline = self->set_indirect_infos0.get(),
    });
    self->loop_task_graph.add_task(ClearDrawFlagsTask{
        .views = std::array{
            daxa::attachment_view(ClearDrawFlagsH::AT.chunks, self->task_chunks),
            daxa::attachment_view(ClearDrawFlagsH::AT.brick_data, self->task_brick_data),
            daxa::attachment_view(ClearDrawFlagsH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
            daxa::attachment_view(ClearDrawFlagsH::AT.indirect_info, task_indirect_infos),
        },
        .pipeline = self->clear_draw_flags_pipeline.get(),
    });

    task_fill_buffer(self->loop_task_graph, self->task_visible_brick_instance_allocator, uint32_t{0});
    clear_task_buffers(self->loop_task_graph, std::array<daxa::TaskBufferView, 1>{self->task_brick_visibility_bits}, std::array{daxa::BufferClearInfo{.size = sizeof(uint32_t) * (MAX_BRICK_INSTANCE_COUNT + 1)}});
    self->loop_task_graph.add_task(AnalyzeVisbufferTask{
        .views = std::array{
            daxa::attachment_view(AnalyzeVisbufferH::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(AnalyzeVisbufferH::AT.chunks, self->task_chunks),
            daxa::attachment_view(AnalyzeVisbufferH::AT.brick_data, self->task_brick_data),
            daxa::attachment_view(AnalyzeVisbufferH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
            daxa::attachment_view(AnalyzeVisbufferH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
            daxa::attachment_view(AnalyzeVisbufferH::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
            daxa::attachment_view(AnalyzeVisbufferH::AT.brick_visibility_bits, self->task_brick_visibility_bits),
            daxa::attachment_view(AnalyzeVisbufferH::AT.visible_brick_instance_allocator, self->task_visible_brick_instance_allocator),
            daxa::attachment_view(AnalyzeVisbufferH::AT.visbuffer64, task_visbuffer64),
        },
        .pipeline = self->analyze_visbuffer_pipeline.get(),
    });

    auto color = self->loop_task_graph.create_transient_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
        .name = "color",
    });
    auto depth = self->loop_task_graph.create_transient_image({
        .format = daxa::Format::R32_SFLOAT,
        .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
        .name = "depth",
    });
    auto motion_vectors = self->loop_task_graph.create_transient_image({
        .format = daxa::Format::R16G16_SFLOAT,
        .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
        .name = "motion_vectors",
    });

    self->loop_task_graph.add_task(ShadeVisbufferTask{
        .views = std::array{
            daxa::attachment_view(ShadeVisbufferH::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(ShadeVisbufferH::AT.chunks, self->task_chunks),
            daxa::attachment_view(ShadeVisbufferH::AT.brick_data, self->task_brick_data),
            daxa::attachment_view(ShadeVisbufferH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
            daxa::attachment_view(ShadeVisbufferH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
            daxa::attachment_view(ShadeVisbufferH::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
            daxa::attachment_view(ShadeVisbufferH::AT.visbuffer64, self->draw_from_observer ? task_observer_visbuffer64 : task_visbuffer64),
#if ENABLE_DEBUG_VIS
            daxa::attachment_view(ShadeVisbufferH::AT.debug_overdraw, task_debug_overdraw),
#endif
            daxa::attachment_view(ShadeVisbufferH::AT.color, color),
            daxa::attachment_view(ShadeVisbufferH::AT.depth, depth),
            daxa::attachment_view(ShadeVisbufferH::AT.motion_vectors, motion_vectors),
        },
        .pipeline = self->shade_visbuffer_pipeline.get(),
    });

    self->loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_READ, self->task_visible_brick_instance_allocator),
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, self->task_brick_instance_allocator),
        },
        .task = [self](daxa::TaskInterface const &ti) {
            ti.recorder.copy_buffer_to_buffer({
                .src_buffer = ti.get(daxa::TaskBufferAttachmentIndex{0}).ids[0],
                .dst_buffer = ti.get(daxa::TaskBufferAttachmentIndex{1}).ids[0],
                .size = std::min(sizeof(BrickInstance) * (self->gpu_input.render_size.x * self->gpu_input.render_size.y + 1), sizeof(BrickInstance) * (MAX_BRICK_INSTANCE_COUNT + 1)),
            });
        },
        .name = "copy state",
    });

    auto upscaled_image = [&]() {
        if (self->use_fsr2 && !self->draw_from_observer) {
            auto upscaled_image = self->loop_task_graph.create_transient_image({
                .format = daxa::Format::R16G16B16A16_SFLOAT,
                .size = {self->gpu_input.render_size.x, self->gpu_input.render_size.y, 1},
                .name = "upscaled_image",
            });
            self->loop_task_graph.add_task({
                .attachments = {
                    daxa::inl_attachment(daxa::TaskImageAccess::COMPUTE_SHADER_SAMPLED, daxa::ImageViewType::REGULAR_2D, color),
                    daxa::inl_attachment(daxa::TaskImageAccess::COMPUTE_SHADER_SAMPLED, daxa::ImageViewType::REGULAR_2D, depth),
                    daxa::inl_attachment(daxa::TaskImageAccess::COMPUTE_SHADER_SAMPLED, daxa::ImageViewType::REGULAR_2D, motion_vectors),
                    daxa::inl_attachment(daxa::TaskImageAccess::COMPUTE_SHADER_STORAGE_WRITE_ONLY, daxa::ImageViewType::REGULAR_2D, upscaled_image),
                },
                .task = [=](daxa::TaskInterface const &ti) {
                    self->fsr2_context.upscale(
                        ti.recorder,
                        daxa::UpscaleInfo{
                            .color = ti.get(daxa::TaskImageAttachmentIndex{0}).ids[0],
                            .depth = ti.get(daxa::TaskImageAttachmentIndex{1}).ids[0],
                            .motion_vectors = ti.get(daxa::TaskImageAttachmentIndex{2}).ids[0],
                            .output = ti.get(daxa::TaskImageAttachmentIndex{3}).ids[0],
                            .should_reset = self->gpu_input.frame_index == 0,
                            .delta_time = self->gpu_input.delta_time,
                            .jitter = self->gpu_input.jitter,
                            .camera_info = {
                                .near_plane = 0.01f,
                                .far_plane = {},
                                .vertical_fov = 74.0f * (3.14159f / 180.0f), // TODO...
                            },
                        });
                },
            });
            return upscaled_image;
        } else {
            return color;
        }
    }();

    self->loop_task_graph.add_task(PostProcessingTask{
        .views = std::array{
            daxa::attachment_view(PostProcessingH::AT.render_target, self->task_swapchain_image),
            daxa::attachment_view(PostProcessingH::AT.color, upscaled_image),
        },
        .pipeline = self->post_processing_pipeline.get(),
    });

    self->loop_task_graph.add_task(DebugLinesTask{
        .views = std::array{
            daxa::attachment_view(DebugLinesH::AT.render_target, self->task_swapchain_image),
            daxa::attachment_view(DebugLinesH::AT.gpu_input, task_input_buffer),
        },
        .pipeline = self->debug_lines_pipeline.get(),
        .debug_lines = &self->debug_lines,
    });

    self->loop_task_graph.add_task(DebugPointsTask{
        .views = std::array{
            daxa::attachment_view(DebugLinesH::AT.render_target, self->task_swapchain_image),
            daxa::attachment_view(DebugLinesH::AT.gpu_input, task_input_buffer),
        },
        .pipeline = self->debug_points_pipeline.get(),
        .debug_points = &self->debug_points,
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
    self->needs_record = false;

    self->gpu_input.frame_index = 0;
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

static renderer::Renderer s_instance;

void renderer::init(Renderer &self, void *glfw_window_ptr) {
    self = new State{};
    s_instance = self;

    self->instance = daxa::create_instance({});
    self->device = self->instance.create_device({
        .flags = daxa::DeviceFlags2{
            .mesh_shader_bit = true,
            .image_atomic64 = true,
        },
        .max_allowed_buffers = 1 << 16,
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
        .present_mode = daxa::PresentMode::IMMEDIATE,
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
    self->fsr2_context = daxa::Fsr2Context({
        .device = self->device,
        .size_info = {
            .render_size_x = self->gpu_input.render_size.x,
            .render_size_y = self->gpu_input.render_size.y,
            .display_size_x = self->gpu_input.render_size.x,
            .display_size_y = self->gpu_input.render_size.y,
        },
        .depth_inf = true,
        .depth_inv = true,
        .color_hdr = true,
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
            .write_out_shader_binary = ".out/spv",
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true,
        },
        .name = "my pipeline manager",
    });

    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"clear_draw_flags.glsl"}},
            .push_constant_size = sizeof(ClearDrawFlagsPush),
            .name = "clear_draw_flags",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->clear_draw_flags_pipeline = result.value();
    }

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
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"set_indirect_infos.glsl"}, .compile_options = {.defines = {{"SET_TYPE", "0"}}}},
            .push_constant_size = sizeof(SetIndirectInfosPush),
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
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"set_indirect_infos.glsl"}, .compile_options = {.defines = {{"SET_TYPE", "1"}}}},
            .push_constant_size = sizeof(SetIndirectInfosPush),
            .name = "set_indirect_infos1",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->set_indirect_infos1 = result.value();
    }

    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"set_indirect_infos.glsl"}, .compile_options = {.defines = {{"SET_TYPE", "2"}}}},
            .push_constant_size = sizeof(SetIndirectInfosPush),
            .name = "set_indirect_infos2",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->set_indirect_infos2 = result.value();
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
            .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}, .compile_options = {.defines = {{"DO_DEPTH_CULL", "1"}}, .required_subgroup_size = 32}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}},
            .raster = {.polygon_mode = daxa::PolygonMode::FILL},
            .push_constant_size = sizeof(DrawVisbufferPush),
            .name = "draw_visbuffer_depth_cull",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->draw_visbuffer_depth_cull_pipeline = result.value();
    }
    {
        auto result = self->pipeline_manager.add_raster_pipeline({
            .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}, .compile_options = {.defines = {{"DRAW_FROM_OBSERVER", "1"}}, .required_subgroup_size = 32}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}},
            .raster = {.polygon_mode = daxa::PolygonMode::FILL},
            .push_constant_size = sizeof(DrawVisbufferPush),
            .name = "draw_visbuffer_observer",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->draw_visbuffer_observer_pipeline = result.value();
    }
    {
        auto result = self->pipeline_manager.add_raster_pipeline({
            .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}, .compile_options = {.defines = {{"DRAW_FROM_OBSERVER", "1"}, {"DO_DEPTH_CULL", "1"}}, .required_subgroup_size = 32}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}},
            .raster = {.polygon_mode = daxa::PolygonMode::FILL},
            .push_constant_size = sizeof(DrawVisbufferPush),
            .name = "draw_visbuffer_observer",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->draw_visbuffer_observer_depth_cull_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"compute_rasterize.glsl"}},
            .push_constant_size = sizeof(ComputeRasterizePush),
            .name = "compute_rasterize",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->compute_rasterize_pipeline = result.value();
    }
    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"compute_rasterize.glsl"}, .compile_options = {.defines = {{"DO_DEPTH_CULL", "1"}}}},
            .push_constant_size = sizeof(ComputeRasterizePush),
            .name = "compute_rasterize_depth_cull",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->compute_rasterize_depth_cull_pipeline = result.value();
    }
    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"compute_rasterize.glsl"}, .compile_options = {.defines = {{"DRAW_FROM_OBSERVER", "1"}}}},
            .push_constant_size = sizeof(ComputeRasterizePush),
            .name = "compute_rasterize_observer",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->compute_rasterize_observer_pipeline = result.value();
    }
    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"compute_rasterize.glsl"}, .compile_options = {.defines = {{"DRAW_FROM_OBSERVER", "1"}, {"DO_DEPTH_CULL", "1"}}}},
            .push_constant_size = sizeof(ComputeRasterizePush),
            .name = "compute_rasterize_observer",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->compute_rasterize_observer_depth_cull_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"shade_visbuffer.glsl"}},
            .push_constant_size = sizeof(ShadeVisbufferPush),
            .name = "shade_visbuffer",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->shade_visbuffer_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"post_processing.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"post_processing.glsl"}},
            .color_attachments = {{.format = self->swapchain.get_format()}},
            .push_constant_size = sizeof(PostProcessingPush),
            .name = "post_processing",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->post_processing_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"analyze_visbuffer.glsl"}},
            .push_constant_size = sizeof(AnalyzeVisbufferPush),
            .name = "analyze_visbuffer",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->analyze_visbuffer_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"gen_hiz.glsl"}},
            .push_constant_size = sizeof(GenHizPush),
            .name = "gen_hiz",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->gen_hiz_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"debug_shapes.glsl"}, .compile_options = {.defines = {{"DEBUG_LINES", "1"}}}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"debug_shapes.glsl"}, .compile_options = {.defines = {{"DEBUG_LINES", "1"}}}},
            .color_attachments = {{.format = self->swapchain.get_format()}},
            .raster = {.primitive_topology = daxa::PrimitiveTopology::LINE_LIST},
            .push_constant_size = sizeof(DebugLinesPush),
            .name = "debug_lines",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->debug_lines_pipeline = result.value();
    }

    {
        auto result = self->pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"debug_shapes.glsl"}, .compile_options = {.defines = {{"DEBUG_POINTS", "1"}}}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"debug_shapes.glsl"}, .compile_options = {.defines = {{"DEBUG_POINTS", "1"}}}},
            .color_attachments = {{.format = self->swapchain.get_format()}},
            .raster = {.primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST},
            .push_constant_size = sizeof(DebugPointsPush),
            .name = "debug_points",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self->debug_points_pipeline = result.value();
    }

    self->task_swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};

    self->task_chunks = daxa::TaskBuffer({.name = "task_chunks"});
    self->task_brick_meshlet_allocator = daxa::TaskBuffer({.name = "task_brick_meshlet_allocator"});
    self->task_brick_meshlet_metadata = daxa::TaskBuffer({.name = "task_brick_meshlet_metadata"});
    self->task_brick_instance_allocator = daxa::TaskBuffer({.name = "task_brick_instance_allocator"});
    self->task_brick_visibility_bits = daxa::TaskBuffer({.name = "task_brick_visibility_bits"});
    self->task_visible_brick_instance_allocator = daxa::TaskBuffer({.name = "task_visible_brick_instance_allocator"});
    self->task_brick_data = daxa::TaskBuffer({.name = "task_brick_data"});

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
        .size = sizeof(uint32_t) * (MAX_BRICK_INSTANCE_COUNT + 1),
        .name = "brick_visibility_bits",
    });
    self->task_brick_visibility_bits.set_buffers({.buffers = std::array{self->brick_visibility_bits}});

    self->gpu_input.samplers = {
        .llc = self->device.create_sampler({
            .magnification_filter = daxa::Filter::LINEAR,
            .minification_filter = daxa::Filter::LINEAR,
            .mipmap_filter = daxa::Filter::LINEAR,
            .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .name = "llc",
        }),
        .nnc = self->device.create_sampler({
            .magnification_filter = daxa::Filter::NEAREST,
            .minification_filter = daxa::Filter::NEAREST,
            .mipmap_filter = daxa::Filter::NEAREST,
            .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .name = "nnc",
        }),
    };

    self->start_time = Clock::now();
    self->prev_time = Clock::now();
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
    if (!self->visible_brick_instance_allocator.is_empty()) {
        self->device.destroy_buffer(self->visible_brick_instance_allocator);
    }

    self->device.destroy_sampler(std::bit_cast<daxa::SamplerId>(self->gpu_input.samplers.llc));
    self->device.destroy_sampler(std::bit_cast<daxa::SamplerId>(self->gpu_input.samplers.nnc));

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    delete self;
}

void renderer::on_resize(Renderer self, int size_x, int size_y) {
    self->gpu_input.render_size = daxa_u32vec2{uint32_t(size_x), uint32_t(size_y)};
    self->swapchain.resize();
    self->fsr2_context.resize({
        .render_size_x = self->gpu_input.render_size.x,
        .render_size_y = self->gpu_input.render_size.y,
        .display_size_x = self->gpu_input.render_size.x,
        .display_size_y = self->gpu_input.render_size.y,
    });

    if (size_x * size_y == 0) {
        return;
    }
    self->gpu_input.next_lower_po2_render_size = daxa_u32vec2{find_next_lower_po2(self->gpu_input.render_size.x), find_next_lower_po2(self->gpu_input.render_size.y)};

    if (!self->visible_brick_instance_allocator.is_empty()) {
        self->device.destroy_buffer(self->visible_brick_instance_allocator);
    }
    self->visible_brick_instance_allocator = self->device.create_buffer({
        // + 1 for the state at index 0
        .size = sizeof(BrickInstance) * (size_x * size_y + 1),
        .name = "visible_brick_instance_allocator",
    });
    self->task_visible_brick_instance_allocator.set_buffers({.buffers = std::array{self->visible_brick_instance_allocator}});

    auto temp_task_graph = daxa::TaskGraph({
        .device = self->device,
        .name = "temp_task_graph",
    });
    temp_task_graph.use_persistent_buffer(self->task_visible_brick_instance_allocator);
    task_fill_buffer(temp_task_graph, self->task_visible_brick_instance_allocator, uint32_t{0});
    temp_task_graph.submit({});
    temp_task_graph.complete({});
    temp_task_graph.execute({});

    record_tasks(self);
}

void renderer::draw(Renderer self, player::Player player, voxel_world::VoxelWorld voxel_world) {
    if (self->needs_update) {
        {
            auto temp_task_graph = daxa::TaskGraph({
                .device = s_instance->device,
                .name = "update_task_graph",
            });
            temp_task_graph.use_persistent_buffer(self->task_brick_data);
            temp_task_graph.use_persistent_buffer(self->task_chunks);

            temp_task_graph.use_persistent_buffer(self->task_brick_meshlet_allocator);
            temp_task_graph.use_persistent_buffer(self->task_brick_instance_allocator);
            temp_task_graph.use_persistent_buffer(self->task_visible_brick_instance_allocator);

            auto task_input_buffer = temp_task_graph.create_transient_buffer({
                .size = sizeof(GpuInput),
                .name = "gpu_input",
            });
            auto task_indirect_infos = temp_task_graph.create_transient_buffer({
                .size = sizeof(DispatchIndirectStruct) * 4,
                .name = "indirect_infos",
            });
            temp_task_graph.add_task(SetIndirectInfosTask{
                .views = std::array{
                    daxa::attachment_view(SetIndirectInfosH::AT.gpu_input, task_input_buffer),
                    daxa::attachment_view(SetIndirectInfosH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                    daxa::attachment_view(SetIndirectInfosH::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(SetIndirectInfosH::AT.indirect_info, task_indirect_infos),
                },
                .pipeline = self->set_indirect_infos0.get(),
            });
            temp_task_graph.add_task(ClearDrawFlagsTask{
                .views = std::array{
                    daxa::attachment_view(ClearDrawFlagsH::AT.chunks, self->task_chunks),
                    daxa::attachment_view(ClearDrawFlagsH::AT.brick_data, self->task_brick_data),
                    daxa::attachment_view(ClearDrawFlagsH::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                    daxa::attachment_view(ClearDrawFlagsH::AT.indirect_info, task_indirect_infos),
                },
                .pipeline = self->clear_draw_flags_pipeline.get(),
            });

            task_fill_buffer(temp_task_graph, self->task_brick_meshlet_allocator, daxa_u32vec2{});
            task_fill_buffer(temp_task_graph, self->task_brick_instance_allocator, uint32_t{0});
            task_fill_buffer(temp_task_graph, self->task_visible_brick_instance_allocator, uint32_t{0});

            temp_task_graph.submit({});
            temp_task_graph.complete({});
            temp_task_graph.execute({});
        }

        self->task_brick_data.set_buffers({.buffers = self->tracked_brick_data});
        auto new_chunk_n = self->tracked_brick_data.size();
        if (new_chunk_n != self->gpu_input.chunk_n) {
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
            .device = s_instance->device,
            .name = "update_task_graph",
        });
        temp_task_graph.use_persistent_buffer(self->task_brick_data);
        temp_task_graph.use_persistent_buffer(self->task_chunks);

        temp_task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, self->task_chunks)},
            .task = [=](daxa::TaskInterface ti) {
                auto staging_input_buffer = ti.device.create_buffer({
                    .size = sizeof(VoxelChunk) * new_chunk_n,
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "staging_input_buffer",
                });
                ti.recorder.destroy_buffer_deferred(staging_input_buffer);
                auto *buffer_ptr = ti.device.get_host_address(staging_input_buffer).value();
                memcpy(buffer_ptr, self->chunks.data(), sizeof(VoxelChunk) * new_chunk_n);
                ti.recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_input_buffer,
                    .dst_buffer = ti.get(self->task_chunks).ids[0],
                    .dst_offset = 0,
                    .size = sizeof(VoxelChunk) * new_chunk_n,
                });
            },
            .name = "fill buffer",
        });

        auto buffer_view = daxa::TaskBufferView{self->task_brick_data};
        temp_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, buffer_view),
            },
            .task = [=](daxa::TaskInterface const &ti) {
                for (auto chunk : self->chunks_to_update) {
                    auto brick_count = chunk->brick_count;

                    auto meshes_size = round_up_div((sizeof(VoxelBrickMesh) * brick_count), 128) * 128;
                    auto bitmasks_size = round_up_div((sizeof(VoxelBrickBitmask) * brick_count), 128) * 128;
                    auto pos_scl_size = round_up_div((sizeof(daxa_i32vec4) * brick_count), 128) * 128;
                    auto flags_size = round_up_div((sizeof(daxa_u32) * brick_count), 128) * 128;
                    auto attribs_size = round_up_div((sizeof(VoxelAttribBrick) * brick_count), 128) * 128;

                    auto meshes_offset = size_t{0};
                    auto bitmasks_offset = meshes_offset + meshes_size;
                    auto pos_scl_offset = bitmasks_offset + bitmasks_size;
                    auto attribs_offset = pos_scl_offset + pos_scl_size;
                    auto flags_offset = attribs_offset + attribs_size;
                    auto total_size = flags_offset + flags_size;

                    auto upload = [&ti](daxa::BufferId dst, void const *data, uint64_t size, uint64_t dst_offset) {
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
                            .dst_buffer = dst,
                            .dst_offset = dst_offset,
                            .size = size,
                        });
                    };
                    auto clear = [&ti](daxa::BufferId dst, uint64_t size, uint64_t dst_offset) {
                        auto staging_input_buffer = ti.device.create_buffer({
                            .size = size,
                            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                            .name = "staging_input_buffer",
                        });
                        ti.recorder.destroy_buffer_deferred(staging_input_buffer);
                        auto *buffer_ptr = ti.device.get_host_address(staging_input_buffer).value();
                        memset(buffer_ptr, 0, size);
                        ti.recorder.copy_buffer_to_buffer({
                            .src_buffer = staging_input_buffer,
                            .dst_buffer = dst,
                            .dst_offset = dst_offset,
                            .size = size,
                        });
                    };

                    upload(ti.get(buffer_view).ids[chunk->tracked_index], chunk->bitmasks.data(), sizeof(VoxelBrickBitmask) * brick_count, bitmasks_offset);
                    upload(ti.get(buffer_view).ids[chunk->tracked_index], chunk->positions.data(), sizeof(daxa_i32vec4) * brick_count, pos_scl_offset);
                    upload(ti.get(buffer_view).ids[chunk->tracked_index], chunk->attribs.data(), sizeof(VoxelAttribBrick) * brick_count, attribs_offset);
                    clear(ti.get(buffer_view).ids[chunk->tracked_index], sizeof(daxa_u32) * brick_count, flags_offset);
                }
            },
            .name = "upload bricks",
        });

        temp_task_graph.submit({});
        temp_task_graph.complete({});
        temp_task_graph.execute({});
        self->needs_update = false;
    }

    {
        auto new_draw_from_observer = player::should_draw_from_observer(player);
        if (self->draw_from_observer != new_draw_from_observer) {
            self->draw_from_observer = new_draw_from_observer;
            self->needs_record = true;
        }
    }

    if (self->needs_record) {
        record_tasks(self);
    }

    auto now = Clock::now();
    auto time = std::chrono::duration<float>(now - self->start_time).count();
    auto delta_time = std::chrono::duration<float>(now - self->prev_time).count();
    self->gpu_input.time = time;
    self->gpu_input.delta_time = delta_time;
    self->prev_time = now;

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
        ImGui::Begin("Debug");
        ImGui::Text("%.3f ms (%.3f fps)", delta_time * 1000.0f, 1.0f / delta_time);
        ImGui::Text("m pos = %.3f %.3f %.3f", self->gpu_input.cam.view_to_world.w.x, self->gpu_input.cam.view_to_world.w.y, self->gpu_input.cam.view_to_world.w.z);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Main camera position");
        }
        ImGui::Text("o pos = %.3f %.3f %.3f", self->gpu_input.observer_cam.view_to_world.w.x, self->gpu_input.observer_cam.view_to_world.w.y, self->gpu_input.observer_cam.view_to_world.w.z);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Observer camera position");
        }
        {
            ImGui::Text("Keybinds:");
            ImGui::Text("ESC = Toggle capture mouse and keyboard");
            ImGui::Text("WASD/SPACE/CONTROL = Move current camera");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("* corresponds to Forward, Left, Backward, Right, Up, Down");
            }
            ImGui::Text("SHIFT = Sprint");
            ImGui::Text("Q = Toggle up movement direction");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("* Toggles the up direction between global up and relative up");
            }
            ImGui::Text("P = Toggle observer camera view");
            ImGui::Text("O = Teleport observer camera to main camera");
            ImGui::Text("N = Control main camera from observer *");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("* only if already in observer camera view");
            }
            ImGui::Text("L = Toggle FSR2");
        }
        ImGui::End();
        ImGui::Render();
    }

    if (self->use_fsr2) {
        self->gpu_input.jitter = self->fsr2_context.get_jitter(self->gpu_input.frame_index);
    } else {
        self->gpu_input.jitter = {0, 0};
    }
    player::get_camera(player, &self->gpu_input.cam, &self->gpu_input);
    player::get_observer_camera(player, &self->gpu_input.observer_cam, &self->gpu_input);

    if (self->tracked_brick_data.empty()) {
        self->loop_empty_task_graph.execute({});
    } else {
        self->loop_task_graph.execute({});
    }
    self->device.collect_garbage();

    self->debug_lines.clear();
    self->debug_points.clear();
    self->tracked_brick_data.clear();
    self->chunks_to_update.clear();

    ++self->gpu_input.frame_index;
}

void renderer::toggle_fsr2(Renderer self) {
    self->use_fsr2 = !self->use_fsr2;
    self->needs_record = true;
}

void renderer::submit_debug_lines(float const *lines, int line_n) {
    Renderer self = s_instance;

    self->debug_lines.reserve(self->debug_lines.size() + line_n);
    for (int i = 0; i < line_n; ++i) {
        auto point = std::array{
            daxa_f32vec3{
                lines[i * 9 + 0],
                lines[i * 9 + 1],
                lines[i * 9 + 2],
            },
            daxa_f32vec3{
                lines[i * 9 + 3],
                lines[i * 9 + 4],
                lines[i * 9 + 5],
            },
            daxa_f32vec3{
                lines[i * 9 + 6],
                lines[i * 9 + 7],
                lines[i * 9 + 8],
            },
        };
        self->debug_lines.push_back(point);
    }
}

void renderer::submit_debug_points(float const *points, int point_n) {
    Renderer self = s_instance;

    self->debug_points.reserve(self->debug_points.size() + point_n);
    for (int i = 0; i < point_n; ++i) {
        auto point = std::array{
            daxa_f32vec3{
                points[i * 9 + 0],
                points[i * 9 + 1],
                points[i * 9 + 2],
            },
            daxa_f32vec3{
                points[i * 9 + 3],
                points[i * 9 + 4],
                points[i * 9 + 5],
            },
            daxa_f32vec3{
                points[i * 9 + 6],
                points[i * 9 + 7],
                points[i * 9 + 8],
            },
        };
        self->debug_points.push_back(point);
    }
}

void renderer::init(Chunk &self) {
    self = new ChunkState{};
}

void renderer::deinit(Chunk self) {
    if (!self->brick_data.is_empty()) {
        s_instance->device.destroy_buffer(self->brick_data);
    }
}

void renderer::update(Chunk self, int brick_count, int const *surface_brick_indices, VoxelBrickBitmask const *bitmasks, VoxelAttribBrick const *const *attribs, int const *positions) {
    self->needs_update = true;
    self->brick_count = brick_count;

    self->bitmasks.clear();
    self->bitmasks.reserve(brick_count);
    for (int i = 0; i < brick_count; ++i) {
        int brick_index = surface_brick_indices[i];
        self->bitmasks.push_back(bitmasks[brick_index]);
    }

    self->attribs.clear();
    self->attribs.reserve(brick_count);
    for (int i = 0; i < brick_count; ++i) {
        int brick_index = surface_brick_indices[i];
        self->attribs.push_back(*attribs[brick_index]);
    }

    self->positions.clear();
    self->positions.reserve(brick_count * 4);
    for (int i = 0; i < brick_count; ++i) {
        int brick_index = surface_brick_indices[i];
        self->positions.push_back(positions[brick_index * 4 + 0]);
        self->positions.push_back(positions[brick_index * 4 + 1]);
        self->positions.push_back(positions[brick_index * 4 + 2]);
        self->positions.push_back(positions[brick_index * 4 + 3]);
    }
}

void renderer::render_chunk(Chunk self, float const *pos) {
    auto brick_count = self->brick_count;

    auto meshes_size = round_up_div((sizeof(VoxelBrickMesh) * brick_count), 128) * 128;
    auto bitmasks_size = round_up_div((sizeof(VoxelBrickBitmask) * brick_count), 128) * 128;
    auto pos_scl_size = round_up_div((sizeof(daxa_i32vec4) * brick_count), 128) * 128;
    auto flags_size = round_up_div((sizeof(daxa_u32) * brick_count), 128) * 128;
    auto attribs_size = round_up_div((sizeof(VoxelAttribBrick) * brick_count), 128) * 128;

    auto meshes_offset = size_t{0};
    auto bitmasks_offset = meshes_offset + meshes_size;
    auto pos_scl_offset = bitmasks_offset + bitmasks_size;
    auto attribs_offset = pos_scl_offset + pos_scl_size;
    auto flags_offset = attribs_offset + attribs_size;
    auto total_size = flags_offset + flags_size;

    if (self->needs_update) {
        if (!self->brick_data.is_empty()) {
            s_instance->device.destroy_buffer(self->brick_data);
        }
        self->brick_data = s_instance->device.create_buffer({
            .size = total_size,
            .name = "brick_data",
        });
    }

    // The buffer will be at the current size of tracked buffers.
    auto tracked_chunk_index = s_instance->tracked_brick_data.size();
    s_instance->tracked_brick_data.push_back(self->brick_data);

    if (tracked_chunk_index != self->tracked_index) {
        self->tracked_index = tracked_chunk_index;
        s_instance->needs_update = true;

        if (tracked_chunk_index >= s_instance->chunks.size()) {
            s_instance->chunks.resize(tracked_chunk_index + 1);
        }
    }

    s_instance->chunks[tracked_chunk_index] = VoxelChunk{
        .bitmasks = s_instance->device.get_device_address(self->brick_data).value() + bitmasks_offset,
        .meshes = s_instance->device.get_device_address(self->brick_data).value() + meshes_offset,
        .pos_scl = s_instance->device.get_device_address(self->brick_data).value() + pos_scl_offset,
        .attribs = s_instance->device.get_device_address(self->brick_data).value() + attribs_offset,
        .flags = s_instance->device.get_device_address(self->brick_data).value() + flags_offset,
        .brick_n = uint32_t(brick_count),
        .pos = {pos[0], pos[1], pos[2]},
    };

    if (self->needs_update) {
        s_instance->needs_update = true;
        self->needs_update = false;
        s_instance->chunks_to_update.push_back(self);
    }
}
