#include "shared.inl"
#include "renderer.hpp"
#include "../player.hpp"
#include "utilities/debug.hpp"
#include "voxels/defs.inl"
#include <daxa/c/core.h>
#include <daxa/command_recorder.hpp>
#include <daxa/gpu_resources.hpp>
#include <daxa/types.hpp>
#include <daxa/utils/task_graph_types.hpp>
#include <imgui.h>
#include <voxels/voxel_mesh.inl>
#include <voxels/voxel_world.hpp>
#include <string_view>

#include <daxa/daxa.hpp>
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/imgui.hpp>
#include <daxa/utils/fsr2.hpp>

#include "debug_shapes.inl"
#include "voxel_rasterizer/voxel_rasterizer.inl"
#include "voxel_raytracer/voxel_raytracer.inl"

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

#undef OPAQUE

struct renderer::ChunkState {
    daxa::BufferId brick_data;
    daxa::BufferId blas_buffer;
    daxa::BufferId blas_scratch_buffer;
    daxa::BlasId blas;
    daxa::BlasBuildInfo blas_build_info;

    uint32_t brick_count;
    uint32_t tracked_index = 0xffffffff;
    std::vector<VoxelBrickBitmask> bitmasks;
    std::vector<VoxelRenderAttribBrick> attribs;
    std::vector<int> positions;
    std::vector<Aabb> aabbs;
    bool needs_update = false;
};

using Clock = std::chrono::steady_clock;

struct renderer::State {
    GpuContext gpu_context;
    daxa::ImGuiRenderer imgui_renderer;
    daxa::Fsr2Context fsr2_context;

    daxa::TaskGraph loop_task_graph;
    daxa::TaskGraph loop_empty_task_graph;

    std::vector<Chunk> chunks_to_update;
    std::vector<VoxelChunk> chunks;
    TemporalBuffer chunks_buffer;

    std::vector<daxa::BufferId> tracked_brick_data;
    daxa::TaskBuffer task_brick_data;

    std::vector<daxa::BlasId> tracked_blases;
    daxa::TaskBlas task_chunk_blases;

    TemporalBuffer blas_instances_buffer;
    daxa::TlasBuildInfo tlas_build_info;
    daxa::AccelerationStructureBuildSizesInfo tlas_build_sizes;
    std::array<daxa::TlasInstanceInfo, 1> blas_instance_info;
    daxa::BufferId tlas_buffer;
    daxa::TlasId tlas;
    daxa::TaskTlas task_tlas;

    GpuInput gpu_input{};
    Clock::time_point start_time;
    Clock::time_point prev_time;

    DebugShapeRenderer debug_shapes;
    VoxelRasterizer voxel_rasterizer;

    bool use_fsr2 = false;
    bool draw_with_rt = false;
    bool draw_from_observer = false;
    bool needs_record = false;
    bool needs_update = false;

    std::array<float, 100> delta_times;
    int delta_times_offset = 0;
    int delta_times_count = 0;
};

void record_tasks(renderer::Renderer self) {
    {
        self->loop_empty_task_graph = daxa::TaskGraph({
            .device = self->gpu_context.device,
            .swapchain = self->gpu_context.swapchain,
            .name = "loop_empty",
        });
        self->loop_empty_task_graph.use_persistent_image(self->gpu_context.task_swapchain_image);

        clear_task_images(self->loop_empty_task_graph, std::array<daxa::TaskImageView, 1>{self->gpu_context.task_swapchain_image}, std::array<daxa::ClearValue, 1>{std::array<uint32_t, 4>{0, 0, 0, 0}});

        self->loop_empty_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, daxa::ImageViewType::REGULAR_2D, self->gpu_context.task_swapchain_image),
            },
            .task = [self](daxa::TaskInterface const &ti) {
                auto swapchain_image = self->gpu_context.task_swapchain_image.get_state().images[0];
                self->imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, swapchain_image, self->gpu_input.render_size.x, self->gpu_input.render_size.y);
            },
            .name = "ImGui draw",
        });

        self->loop_empty_task_graph.submit({});
        self->loop_empty_task_graph.present({});
        self->loop_empty_task_graph.complete({});
    }

    self->blas_instances_buffer = self->gpu_context.find_or_add_temporal_buffer({
        .size = sizeof(daxa_BlasInstanceData) * MAX_CHUNK_COUNT,
        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE, // TODO
        .name = "blas instances array buffer",
    });

    self->loop_task_graph = daxa::TaskGraph({
        .device = self->gpu_context.device,
        .swapchain = self->gpu_context.swapchain,
        .name = "loop",
    });
    self->loop_task_graph.use_persistent_buffer(self->gpu_context.task_input_buffer);
    self->loop_task_graph.use_persistent_image(self->gpu_context.task_swapchain_image);
    self->loop_task_graph.use_persistent_buffer(self->chunks_buffer);
    self->loop_task_graph.use_persistent_buffer(self->task_brick_data);
    self->loop_task_graph.use_persistent_blas(self->task_chunk_blases);
    self->loop_task_graph.use_persistent_tlas(self->task_tlas);

    auto task_input_buffer = self->gpu_context.task_input_buffer;
    self->loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_input_buffer),
        },
        .task = [self, task_input_buffer](daxa::TaskInterface const &ti) {
            allocate_fill_copy(ti, self->gpu_input, ti.get(task_input_buffer));
        },
        .name = "GpuInputUploadTransferTask",
    });

    auto [color, depth, motion_vectors] = [&]() {
        if (self->draw_with_rt) {
            return renderer::raytrace(self->gpu_context, self->loop_task_graph, self->task_tlas, self->chunks_buffer, self->task_brick_data, self->gpu_input.chunk_n, self->draw_from_observer);
        } else {
            return render(&self->voxel_rasterizer, self->gpu_context, self->loop_task_graph, self->chunks_buffer, self->task_brick_data, self->gpu_input.chunk_n, self->draw_from_observer);
        }
    }();

    auto upscaled_image = [&]() {
        if (self->use_fsr2 && !self->draw_from_observer) {
            auto upscaled_image = self->loop_task_graph.create_transient_image({
                .format = daxa::Format::R16G16B16A16_SFLOAT,
                .size = {self->gpu_context.output_resolution.x, self->gpu_context.output_resolution.y, 1},
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

    draw_debug_shapes(self->gpu_context, self->loop_task_graph, upscaled_image, &self->debug_shapes);

    self->gpu_context.add(RasterTask<PostProcessing::Task, PostProcessingPush, NoTaskInfo>{
        .vert_source = daxa::ShaderFile{"FULL_SCREEN_TRIANGLE_VERTEX_SHADER"},
        .frag_source = daxa::ShaderFile{"post_processing.glsl"},
        .color_attachments = {{.format = self->gpu_context.swapchain.get_format()}},
        .views = std::array{
            daxa::attachment_view(PostProcessing::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(PostProcessing::AT.render_target, self->gpu_context.task_swapchain_image),
            daxa::attachment_view(PostProcessing::AT.color, upscaled_image),
        },
        .callback_ = [](daxa::TaskInterface const &ti, daxa::RasterPipeline &pipeline, PostProcessingPush &push, NoTaskInfo const &) {
            auto render_image = ti.get(PostProcessing::AT.render_target).ids[0];
            auto const image_info = ti.device.info_image(render_image).value();
            auto renderpass_recorder = std::move(ti.recorder).begin_renderpass({
                .color_attachments = {{.image_view = render_image.default_view(), .load_op = daxa::AttachmentLoadOp::DONT_CARE}},
                .render_area = {.x = 0, .y = 0, .width = image_info.size.x, .height = image_info.size.y},
            });
            renderpass_recorder.set_pipeline(pipeline);
            push.image_size = {image_info.size.x, image_info.size.y};
            set_push_constant(ti, renderpass_recorder, push);
            renderpass_recorder.draw({.vertex_count = 3});
            ti.recorder = std::move(renderpass_recorder).end_renderpass();
        },
        .task_graph = &self->loop_task_graph,
    });

    self->loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, daxa::ImageViewType::REGULAR_2D, self->gpu_context.task_swapchain_image),
        },
        .task = [self](daxa::TaskInterface const &ti) {
            auto swapchain_image = self->gpu_context.task_swapchain_image.get_state().images[0];
            auto const image_info = ti.device.info_image(swapchain_image).value();
            self->imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, swapchain_image, image_info.size.x, image_info.size.y);
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

void renderer::init(Renderer &self, void *glfw_window_ptr) {
    self = new State{};

    auto *native_window_handle = get_native_handle(glfw_window_ptr);
    auto native_window_platform = get_native_platform(glfw_window_ptr);
    self->gpu_context.create_swapchain({
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
        .device = self->gpu_context.device,
        .format = self->gpu_context.swapchain.get_format(),
        .context = ImGui::GetCurrentContext(),
        .use_custom_config = false,
    });
    self->fsr2_context = daxa::Fsr2Context({
        .device = self->gpu_context.device,
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

    self->task_brick_data = daxa::TaskBuffer({.name = "task_brick_data"});
    self->task_chunk_blases = daxa::TaskBlas({.name = "task_chunk_blases"});
    self->task_tlas = daxa::TaskTlas({.name = "task_tlas"});

    self->chunks_buffer = self->gpu_context.find_or_add_temporal_buffer({
        .size = sizeof(VoxelChunk) * 1,
        .name = "chunks_buffer",
    });

    self->start_time = Clock::now();
    self->prev_time = Clock::now();

    self->debug_shapes.draw_from_observer = &self->draw_from_observer;
}

void renderer::deinit(Renderer self) {
    self->gpu_context.device.wait_idle();
    self->gpu_context.device.collect_garbage();
    self->gpu_context.device.destroy_tlas(self->tlas);
    self->gpu_context.device.destroy_buffer(self->tlas_buffer);

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    delete self;
}

void renderer::on_resize(Renderer self, int size_x, int size_y) {
    self->gpu_context.output_resolution = daxa_u32vec2{uint32_t(size_x), uint32_t(size_y)};
    self->gpu_context.render_resolution = daxa_u32vec2{uint32_t(size_x), uint32_t(size_y)};
    self->gpu_context.next_lower_po2_render_size = daxa_u32vec2{find_next_lower_po2(self->gpu_context.render_resolution.x), find_next_lower_po2(self->gpu_context.render_resolution.y)};

    self->gpu_input.render_size = self->gpu_context.render_resolution;
    self->gpu_input.next_lower_po2_render_size = self->gpu_context.next_lower_po2_render_size;

    self->gpu_context.swapchain.resize();
    self->fsr2_context.resize({
        .render_size_x = self->gpu_context.render_resolution.x,
        .render_size_y = self->gpu_context.render_resolution.y,
        .display_size_x = self->gpu_context.output_resolution.x,
        .display_size_y = self->gpu_context.output_resolution.y,
    });

    if (size_x * size_y == 0) {
        return;
    }

    on_resize(&self->voxel_rasterizer, self->gpu_context, size_x, size_y);

    record_tasks(self);
}

void update(renderer::Renderer self) {
    self->task_brick_data.set_buffers({.buffers = self->tracked_brick_data});
    self->task_chunk_blases.set_blas({.blas = self->tracked_blases});

    if (self->gpu_input.chunk_n != 0) {
        update(&self->voxel_rasterizer, self->gpu_context, self->chunks_buffer, self->task_brick_data);
    }

    auto new_chunk_n = self->tracked_brick_data.size();
    if (new_chunk_n != self->gpu_input.chunk_n) {
        self->gpu_context.resize_temporal_buffer(self->chunks_buffer, sizeof(VoxelChunk) * new_chunk_n);
    }
    self->gpu_input.chunk_n = new_chunk_n;
    {
        // update tlas
        auto *blas_instances = self->gpu_context.device.get_host_address_as<daxa_BlasInstanceData>(self->blas_instances_buffer.resource_id).value();
        int scl = -4 + 8;
#define SCL (float(1 << scl) / float(1 << 8))

        for (auto chunk : self->chunks_to_update) {
            auto &gpu_chunk = self->chunks[chunk->tracked_index];
            blas_instances[chunk->tracked_index] = daxa_BlasInstanceData{
                .transform = {
                    {1, 0, 0, gpu_chunk.pos.x * float(VOXEL_CHUNK_SIZE) * SCL},
                    {0, 1, 0, gpu_chunk.pos.y * float(VOXEL_CHUNK_SIZE) * SCL},
                    {0, 0, 1, gpu_chunk.pos.z * float(VOXEL_CHUNK_SIZE) * SCL},
                },
                .instance_custom_index = chunk->tracked_index,
                .mask = chunk->brick_count == 0 ? 0x00u : 0xff, // chunk->cull_mask,
                .instance_shader_binding_table_record_offset = 0,
                .flags = {},
                .blas_device_address = self->gpu_context.device.get_device_address(chunk->blas).value(),
            };
        }

        self->blas_instance_info = std::array{
            daxa::TlasInstanceInfo{
                .data = self->gpu_context.device.get_device_address(self->blas_instances_buffer.resource_id).value(),
                .count = static_cast<uint32_t>(self->gpu_input.chunk_n),
                .is_data_array_of_pointers = false, // Buffer contains flat array of instances, not an array of pointers to instances.
                .flags = daxa::GeometryFlagBits::OPAQUE,
            },
        };
        self->tlas_build_info = daxa::TlasBuildInfo{
            .flags = daxa::AccelerationStructureBuildFlagBits::PREFER_FAST_TRACE,
            .dst_tlas = {}, // Ignored in get_acceleration_structure_build_sizes.
            .instances = self->blas_instance_info,
            .scratch_data = {}, // Ignored in get_acceleration_structure_build_sizes.
        };
        self->tlas_build_sizes = self->gpu_context.device.get_tlas_build_sizes(self->tlas_build_info);
        if (!self->tlas.is_empty()) {
            self->gpu_context.device.destroy_tlas(self->tlas);
            self->gpu_context.device.destroy_buffer(self->tlas_buffer);
        }
        self->tlas_buffer = self->gpu_context.device.create_buffer({
            .size = self->tlas_build_sizes.acceleration_structure_size,
            .name = "tlas_buffer",
        });
        self->tlas = self->gpu_context.device.create_tlas_from_buffer({
            .tlas_info = {
                .size = self->tlas_build_sizes.acceleration_structure_size,
                .name = "tlas",
            },
            .buffer_id = self->tlas_buffer,
            .offset = 0,
        });
        self->task_tlas.set_tlas({.tlas = std::array{self->tlas}});
    }

    {
        auto temp_task_graph = daxa::TaskGraph({
            .device = self->gpu_context.device,
            .name = "update_task_graph",
        });
        temp_task_graph.use_persistent_buffer(self->task_brick_data);
        temp_task_graph.use_persistent_blas(self->task_chunk_blases);
        temp_task_graph.use_persistent_tlas(self->task_tlas);
        temp_task_graph.use_persistent_buffer(self->chunks_buffer);

        temp_task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, self->chunks_buffer)},
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
                    .dst_buffer = ti.get(self->chunks_buffer).ids[0],
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
                    auto aabb_size = round_up_div((sizeof(Aabb) * brick_count), 128) * 128;
                    auto flags_size = round_up_div((sizeof(daxa_u32) * brick_count), 128) * 128;
                    auto attribs_size = round_up_div((sizeof(VoxelRenderAttribBrick) * brick_count), 128) * 128;

                    auto meshes_offset = size_t{0};
                    auto bitmasks_offset = meshes_offset + meshes_size;
                    auto pos_scl_offset = bitmasks_offset + bitmasks_size;
                    auto aabb_offset = pos_scl_offset + pos_scl_size;
                    auto attribs_offset = aabb_offset + aabb_size;
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
                    upload(ti.get(buffer_view).ids[chunk->tracked_index], chunk->attribs.data(), sizeof(VoxelRenderAttribBrick) * brick_count, attribs_offset);
                    upload(ti.get(buffer_view).ids[chunk->tracked_index], chunk->aabbs.data(), sizeof(Aabb) * brick_count, aabb_offset);
                    clear(ti.get(buffer_view).ids[chunk->tracked_index], sizeof(daxa_u32) * brick_count, flags_offset);
                }
            },
            .name = "upload bricks",
        });

        temp_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_READ, buffer_view),
                daxa::inl_attachment(daxa::TaskBlasAccess::BUILD_WRITE, self->task_chunk_blases),
            },
            .task = [=](daxa::TaskInterface const &ti) {
                for (auto chunk : self->chunks_to_update) {
                    if (!chunk->aabbs.empty()) {
                        auto const &voxel_chunk = self->chunks[chunk->tracked_index];
                        auto geometry = std::array{
                            daxa::BlasAabbGeometryInfo{
                                .data = voxel_chunk.aabbs,
                                .stride = sizeof(Aabb),
                                .count = static_cast<uint32_t>(voxel_chunk.brick_n),
                                .flags = daxa::GeometryFlagBits::OPAQUE,
                            },
                        };
                        chunk->blas_build_info.geometries = geometry;
                        ti.recorder.build_acceleration_structures({
                            .blas_build_infos = std::array{chunk->blas_build_info},
                        });
                        ti.recorder.destroy_buffer_deferred(chunk->blas_scratch_buffer);
                    }
                }
            },
            .name = "blas build",
        });

        temp_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBlasAccess::BUILD_READ, self->task_chunk_blases),
                daxa::inl_attachment(daxa::TaskTlasAccess::BUILD_WRITE, self->task_tlas),
            },
            .task = [=](daxa::TaskInterface const &ti) {
                auto tlas_scratch_buffer = ti.device.create_buffer({
                    .size = self->tlas_build_sizes.build_scratch_size,
                    .name = "tlas_scratch_buffer",
                });
                ti.recorder.destroy_buffer_deferred(tlas_scratch_buffer);
                self->tlas_build_info.dst_tlas = self->tlas;
                self->tlas_build_info.scratch_data = ti.device.get_device_address(tlas_scratch_buffer).value();
                ti.recorder.build_acceleration_structures({
                    .tlas_build_infos = std::array{self->tlas_build_info},
                });
            },
            .name = "tlas build",
        });

        temp_task_graph.submit({});
        temp_task_graph.complete({});
        temp_task_graph.execute({});
    }
    self->needs_update = false;
}

void render_ui(renderer::Renderer self) {
    float avg_delta_time = 0.0f;
    for (int i = 0; i < self->delta_times_count; ++i) {
        avg_delta_time += self->delta_times[i];
    }
    avg_delta_time *= 1.0f / self->delta_times_count;

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Debug");
    ImGui::Text("%.3f ms (%.3f fps)", avg_delta_time * 1000.0f, 1.0f / avg_delta_time);
    ImGui::SameLine();
    ImGui::Text(self->draw_with_rt ? "[RT ON]" : "[RT OFF]");
    ImGui::Text("m pos = %.3f %.3f %.3f", self->gpu_input.cam.view_to_world.w.x, self->gpu_input.cam.view_to_world.w.y, self->gpu_input.cam.view_to_world.w.z);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Main camera position");
    }
    ImGui::Text("o pos = %.3f %.3f %.3f", self->gpu_input.observer_cam.view_to_world.w.x, self->gpu_input.observer_cam.view_to_world.w.y, self->gpu_input.observer_cam.view_to_world.w.z);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Observer camera position");
    }
    {
        ImGui::Text("Inputs:");
        ImGui::Text(" ESC    | Toggle capture mouse and keyboard");
        ImGui::Text(" WASD/SPACE/CONTROL | Move current camera");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("* corresponds to Forward, Left, Backward, Right, Up, Down");
        }
        ImGui::Text(" SHIFT  | Sprint");
        ImGui::Text(" SCROLL | Speed-up/down");
        ImGui::Text(" Q      | Toggle up movement direction");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("* Toggles the up direction between global up and relative up");
        }
        ImGui::Text(" P      | Toggle observer camera view");
        ImGui::Text(" O      | Teleport observer camera to main camera");
        ImGui::Text(" N      | Control main camera from observer *");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("* only if already in observer camera view");
        }
        ImGui::Text(" L      | Toggle FSR2");
        ImGui::Text(" R      | Toggle HWRT");
        ImGui::Text(" F      | Toggle Fly");
        ImGui::Text(" F5     | Toggle Third-person");
        ImGui::Text(" C      | Toggle Fast-placement");
        ImGui::Text(" LMB    | Break voxels");
        ImGui::Text(" RMB    | Place voxels");
    }

    {
        auto format_to_pixel_size = [](daxa::Format format) -> daxa_u32 {
            switch (format) {
            case daxa::Format::R16G16B16_SFLOAT: return 3 * 2;
            case daxa::Format::R16G16B16A16_SFLOAT: return 4 * 2;
            case daxa::Format::R32G32B32_SFLOAT: return 3 * 4;
            default:
            case daxa::Format::R32G32B32A32_SFLOAT: return 4 * 4;
            }
        };

        auto result_size = size_t{};
        struct ResourceInfo {
            std::string_view type;
            std::string name;
            uint64_t size;
        };
        auto debug_gpu_resource_infos = std::vector<ResourceInfo>{};

        auto image_size = [self, &format_to_pixel_size, &result_size, &debug_gpu_resource_infos](daxa::ImageId image) {
            if (image.is_empty()) {
                return;
            }
            auto image_info = self->gpu_context.device.info_image(image).value();
            auto size = format_to_pixel_size(image_info.format) * image_info.size.x * image_info.size.y * image_info.size.z;
            debug_gpu_resource_infos.push_back({
                .type = "image",
                .name = image_info.name.data(),
                .size = size,
            });
            result_size += size;
        };
        auto buffer_size = [self, &result_size, &debug_gpu_resource_infos](daxa::BufferId buffer, bool individual = true) -> size_t {
            if (buffer.is_empty()) {
                return 0;
            }
            auto buffer_info = self->gpu_context.device.info_buffer(buffer).value();
            auto size = buffer_info.size;
            if (individual) {
                debug_gpu_resource_infos.push_back({
                    .type = "buffer",
                    .name = buffer_info.name.data(),
                    .size = buffer_info.size,
                });
            }
            result_size += buffer_info.size;
            return buffer_info.size;
        };
        for (auto &[name, temporal_buffer] : self->gpu_context.temporal_buffers) {
            buffer_size(temporal_buffer.resource_id);
        }
        for (auto &[name, temporal_image] : self->gpu_context.temporal_images) {
            image_size(temporal_image.resource_id);
        }
        buffer_size(self->gpu_context.input_buffer);

        {
            auto size = self->loop_task_graph.get_transient_memory_size();
            debug_gpu_resource_infos.push_back({
                .type = "buffer",
                .name = "Per-frame Transient Memory Buffer",
                .size = size,
            });
            result_size += size;
        }

        std::sort(debug_gpu_resource_infos.begin(), debug_gpu_resource_infos.end(), [](auto const &a, auto const &b) { return a.size > b.size; });

        ImGui::Text("RESOURCES (%f MB)", float(result_size) / 1'000'000.0f);
        for (auto const &info : debug_gpu_resource_infos) {
            ImGui::Text("  %s %f MB (%f %%)", info.name.data(), float(info.size) / 1'000'000.0f, float(info.size) / float(result_size) * 100.0f);
        }
    }
    ImGui::End();

    debug_utils::draw_imgui(g_console, "Console", nullptr);

    ImGui::Render();
}

void renderer::draw(Renderer self, player::Player player, voxel_world::VoxelWorld voxel_world) {
    if (self->needs_update) {
        ::update(self);
    }

    {
        auto new_draw_from_observer = should_draw_from_observer(player);
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

    self->delta_times_offset = (self->delta_times_offset + 1) % self->delta_times.size();
    self->delta_times[self->delta_times_offset] = delta_time;
    self->delta_times_count = std::min<int>(self->delta_times_count + 1, self->delta_times.size());

    auto swapchain_image = self->gpu_context.swapchain.acquire_next_image();
    if (swapchain_image.is_empty()) {
        return;
    }
    auto reload_result = self->gpu_context.pipeline_manager->reload_all();
    if (auto *reload_err = daxa::get_if<daxa::PipelineReloadError>(&reload_result)) {
        add_log(g_console, reload_err->message.c_str());
    }
    self->gpu_context.task_swapchain_image.set_images({.images = std::span{&swapchain_image, 1}});
    render_ui(self);

    if (self->use_fsr2) {
        self->gpu_input.jitter = self->fsr2_context.get_jitter(self->gpu_input.frame_index);
    } else {
        self->gpu_input.jitter = {0, 0};
    }
    get_camera(player, &self->gpu_input.cam, &self->gpu_input);
    get_observer_camera(player, &self->gpu_input.observer_cam, &self->gpu_input);

    if (self->tracked_brick_data.empty()) {
        self->loop_empty_task_graph.execute({});
    } else {
        self->loop_task_graph.execute({});
    }
    self->gpu_context.device.collect_garbage();

    update(&self->debug_shapes);
    self->tracked_brick_data.clear();
    self->tracked_blases.clear();
    self->chunks_to_update.clear();

    ++self->gpu_input.frame_index;
}

void renderer::toggle_fsr2(Renderer self) {
    self->use_fsr2 = !self->use_fsr2;
    self->needs_record = true;
}

void renderer::toggle_rt(Renderer self) {
    self->draw_with_rt = !self->draw_with_rt;
    self->needs_record = true;
}

void renderer::submit_debug_lines(Renderer self, Line const *lines, int line_n) {
    submit_debug_lines(&self->debug_shapes, lines, line_n);
}

void renderer::submit_debug_points(Renderer self, Point const *points, int point_n) {
    submit_debug_points(&self->debug_shapes, points, point_n);
}

void renderer::submit_debug_box_lines(Renderer self, Box const *cubes, int cube_n) {
    submit_debug_box_lines(&self->debug_shapes, cubes, cube_n);
}

auto renderer::create_chunk(Renderer self) -> Chunk {
    return new ChunkState{};
}

void renderer::destroy_chunk(Renderer self, Chunk chunk) {
    if (!chunk->brick_data.is_empty()) {
        self->gpu_context.device.destroy_buffer(chunk->brick_data);
        self->gpu_context.device.destroy_buffer(chunk->blas_buffer);
    }
}

void renderer::update(Chunk self, int brick_count, int const *surface_brick_indices, VoxelBrickBitmask const *bitmasks, VoxelRenderAttribBrick const *const *attribs, int const *positions) {
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
    self->aabbs.clear();
    self->aabbs.reserve(brick_count);
    for (int i = 0; i < brick_count; ++i) {
        int brick_index = surface_brick_indices[i];
        auto px = positions[brick_index * 4 + 0];
        auto py = positions[brick_index * 4 + 1];
        auto pz = positions[brick_index * 4 + 2];
        auto scl = positions[brick_index * 4 + 3] + 8;

        self->positions.push_back(positions[brick_index * 4 + 0]);
        self->positions.push_back(positions[brick_index * 4 + 1]);
        self->positions.push_back(positions[brick_index * 4 + 2]);
        self->positions.push_back(positions[brick_index * 4 + 3]);

#define SCL (float(1 << scl) / float(1 << 8))
        float x0 = float(px * VOXEL_BRICK_SIZE) * SCL;
        float y0 = float(py * VOXEL_BRICK_SIZE) * SCL;
        float z0 = float(pz * VOXEL_BRICK_SIZE) * SCL;
        float x1 = float((px + 1) * VOXEL_BRICK_SIZE) * SCL;
        float y1 = float((py + 1) * VOXEL_BRICK_SIZE) * SCL;
        float z1 = float((pz + 1) * VOXEL_BRICK_SIZE) * SCL;
        self->aabbs.push_back({{x0, y0, z0}, {x1, y1, z1}});
    }
}

void renderer::render_chunk(Renderer self, Chunk chunk, float const *pos) {
    auto brick_count = chunk->brick_count;

    auto meshes_size = round_up_div((sizeof(VoxelBrickMesh) * brick_count), 128) * 128;
    auto bitmasks_size = round_up_div((sizeof(VoxelBrickBitmask) * brick_count), 128) * 128;
    auto pos_scl_size = round_up_div((sizeof(daxa_i32vec4) * brick_count), 128) * 128;
    auto aabb_size = round_up_div((sizeof(Aabb) * brick_count), 128) * 128;
    auto flags_size = round_up_div((sizeof(daxa_u32) * brick_count), 128) * 128;
    auto attribs_size = round_up_div((sizeof(VoxelRenderAttribBrick) * brick_count), 128) * 128;

    auto meshes_offset = size_t{0};
    auto bitmasks_offset = meshes_offset + meshes_size;
    auto pos_scl_offset = bitmasks_offset + bitmasks_size;
    auto aabb_offset = pos_scl_offset + pos_scl_size;
    auto attribs_offset = aabb_offset + aabb_size;
    auto flags_offset = attribs_offset + attribs_size;
    auto total_size = flags_offset + flags_size;

    if (chunk->needs_update) {
        // resize brick data buffer
        if (!chunk->brick_data.is_empty()) {
            self->gpu_context.device.destroy_buffer(chunk->brick_data);
        }
        chunk->brick_data = self->gpu_context.device.create_buffer({
            .size = total_size,
            .name = "brick_data",
        });

        // recreate blas
        auto acceleration_structure_scratch_offset_alignment = self->gpu_context.device.properties().acceleration_structure_properties.value().min_acceleration_structure_scratch_offset_alignment;
        const daxa_u32 ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT = 256; // NOTE: Requested by the spec
        auto geometry = std::array{
            daxa::BlasAabbGeometryInfo{
                .data = self->gpu_context.device.get_device_address(chunk->brick_data).value() + aabb_offset,
                .stride = sizeof(Aabb),
                .count = static_cast<uint32_t>(brick_count),
                .flags = daxa::GeometryFlagBits::OPAQUE,
            },
        };
        chunk->blas_build_info = daxa::BlasBuildInfo{
            .flags = daxa::AccelerationStructureBuildFlagBits::PREFER_FAST_TRACE,
            .dst_blas = {}, // Ignored in get_blas_build_sizes.
            .geometries = geometry,
            .scratch_data = {}, // Ignored in get_blas_build_sizes.
        };
        auto build_size_info = self->gpu_context.device.get_blas_build_sizes(chunk->blas_build_info);
        auto scratch_alignment_size = get_aligned(build_size_info.build_scratch_size, acceleration_structure_scratch_offset_alignment);
        chunk->blas_scratch_buffer = self->gpu_context.device.create_buffer({
            .size = scratch_alignment_size,
            .name = "blas_scratch_buffer",
        });
        chunk->blas_build_info.scratch_data = self->gpu_context.device.get_device_address(chunk->blas_scratch_buffer).value();
        auto build_aligment_size = get_aligned(build_size_info.acceleration_structure_size, ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT);
        if (!chunk->blas_buffer.is_empty()) {
            self->gpu_context.device.destroy_buffer(chunk->blas_buffer);
        }
        chunk->blas_buffer = self->gpu_context.device.create_buffer({
            .size = build_aligment_size,
            .name = "blas_buffer",
        });
        chunk->blas = self->gpu_context.device.create_blas_from_buffer({
            .blas_info = {
                .size = build_size_info.acceleration_structure_size,
                .name = "blas",
            },
            .buffer_id = chunk->blas_buffer,
            .offset = 0,
        });
        chunk->blas_build_info.dst_blas = chunk->blas;
    }

    // The buffer will be at the current size of tracked buffers.
    auto tracked_chunk_index = self->tracked_brick_data.size();
    self->tracked_brick_data.push_back(chunk->brick_data);
    self->tracked_blases.push_back(chunk->blas);

    if (tracked_chunk_index != chunk->tracked_index) {
        chunk->tracked_index = tracked_chunk_index;
        self->needs_update = true;

        if (tracked_chunk_index >= self->chunks.size()) {
            self->chunks.resize(tracked_chunk_index + 1);
        }
    }

    self->chunks[tracked_chunk_index] = VoxelChunk{
        .bitmasks = self->gpu_context.device.get_device_address(chunk->brick_data).value() + bitmasks_offset,
        .meshes = self->gpu_context.device.get_device_address(chunk->brick_data).value() + meshes_offset,
        .pos_scl = self->gpu_context.device.get_device_address(chunk->brick_data).value() + pos_scl_offset,
        .aabbs = self->gpu_context.device.get_device_address(chunk->brick_data).value() + aabb_offset,
        .attribs = self->gpu_context.device.get_device_address(chunk->brick_data).value() + attribs_offset,
        .flags = self->gpu_context.device.get_device_address(chunk->brick_data).value() + flags_offset,
        .brick_n = uint32_t(brick_count),
        .pos = {pos[0], pos[1], pos[2]},
    };

    if (chunk->needs_update) {
        self->needs_update = true;
        chunk->needs_update = false;
        self->chunks_to_update.push_back(chunk);
    }
}
