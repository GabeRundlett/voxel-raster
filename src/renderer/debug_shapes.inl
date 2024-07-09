#pragma once

#include <renderer/shared.inl>

DAXA_DECL_TASK_HEAD_BEGIN(DebugLines)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_DECL_TASK_HEAD_END

struct DebugLinesPush {
    DAXA_TH_BLOB(DebugLines, uses)
    daxa_BufferPtr(daxa_f32vec3) vertex_data;
    daxa_u32 flags;
};

DAXA_DECL_TASK_HEAD_BEGIN(DebugPoints)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_DECL_TASK_HEAD_END

struct DebugPointsPush {
    DAXA_TH_BLOB(DebugPoints, uses)
    daxa_BufferPtr(daxa_f32vec3) vertex_data;
    daxa_u32 flags;
};

#if defined(__cplusplus)

#include <renderer/renderer.hpp>
#include <renderer/utilities/gpu_context.hpp>

namespace renderer {
    struct DebugShapeRenderer {
        bool const *draw_from_observer;
        std::vector<Line> lines = {};
        std::vector<Point> points = {};
    };

    inline void update(DebugShapeRenderer *self) {
        self->lines.clear();
        self->points.clear();
    }

    inline void submit_debug_lines(DebugShapeRenderer *self, Line const *lines, int line_n) {
        self->lines.reserve(self->lines.size() + line_n);
        for (int i = 0; i < line_n; ++i) {
            self->lines.push_back(lines[i]);
        }
    }

    inline void submit_debug_points(DebugShapeRenderer *self, Point const *points, int point_n) {
        self->points.reserve(self->points.size() + point_n);
        for (int i = 0; i < point_n; ++i) {
            self->points.push_back(points[i]);
        }
    }

    inline void submit_debug_box_lines(DebugShapeRenderer *self, Box const *cubes, int cube_n) {
        for (int i = 0; i < cube_n; ++i) {
            auto const &cube = cubes[i];

            auto lines = std::array{
                // clang-format off
            Line{.p0_x = cube.p0_x, .p0_y = cube.p0_y, .p0_z = cube.p0_z,
                .p1_x = cube.p1_x, .p1_y = cube.p0_y, .p1_z = cube.p0_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
            Line{.p0_x = cube.p1_x, .p0_y = cube.p0_y, .p0_z = cube.p0_z,
                .p1_x = cube.p1_x, .p1_y = cube.p1_y, .p1_z = cube.p0_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
            Line{.p0_x = cube.p1_x, .p0_y = cube.p1_y, .p0_z = cube.p0_z,
                .p1_x = cube.p0_x, .p1_y = cube.p1_y, .p1_z = cube.p0_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
            Line{.p0_x = cube.p0_x, .p0_y = cube.p1_y, .p0_z = cube.p0_z,
                .p1_x = cube.p0_x, .p1_y = cube.p0_y, .p1_z = cube.p0_z,
                .r = cube.r, .g = cube.g, .b = cube.b},

            Line{.p0_x = cube.p0_x, .p0_y = cube.p0_y, .p0_z = cube.p1_z,
                .p1_x = cube.p1_x, .p1_y = cube.p0_y, .p1_z = cube.p1_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
            Line{.p0_x = cube.p1_x, .p0_y = cube.p0_y, .p0_z = cube.p1_z,
                .p1_x = cube.p1_x, .p1_y = cube.p1_y, .p1_z = cube.p1_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
            Line{.p0_x = cube.p1_x, .p0_y = cube.p1_y, .p0_z = cube.p1_z,
                .p1_x = cube.p0_x, .p1_y = cube.p1_y, .p1_z = cube.p1_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
            Line{.p0_x = cube.p0_x, .p0_y = cube.p1_y, .p0_z = cube.p1_z,
                .p1_x = cube.p0_x, .p1_y = cube.p0_y, .p1_z = cube.p1_z,
                .r = cube.r, .g = cube.g, .b = cube.b},

            Line{.p0_x = cube.p0_x, .p0_y = cube.p0_y, .p0_z = cube.p0_z,
                .p1_x = cube.p0_x, .p1_y = cube.p0_y, .p1_z = cube.p1_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
            Line{.p0_x = cube.p1_x, .p0_y = cube.p0_y, .p0_z = cube.p0_z,
                .p1_x = cube.p1_x, .p1_y = cube.p0_y, .p1_z = cube.p1_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
            Line{.p0_x = cube.p0_x, .p0_y = cube.p1_y, .p0_z = cube.p0_z,
                .p1_x = cube.p0_x, .p1_y = cube.p1_y, .p1_z = cube.p1_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
            Line{.p0_x = cube.p1_x, .p0_y = cube.p1_y, .p0_z = cube.p0_z,
                .p1_x = cube.p1_x, .p1_y = cube.p1_y, .p1_z = cube.p1_z,
                .r = cube.r, .g = cube.g, .b = cube.b},
                // clang-format on
            };

            submit_debug_lines(self, lines.data(), lines.size());
        }
    }

    inline void draw_debug_shapes(GpuContext &gpu_context, daxa::TaskGraph &task_graph, daxa::TaskImageView render_target, daxa::TaskImageView temp_depth_image, DebugShapeRenderer const *state) {
        auto depth_target = task_graph.create_transient_image({
            .format = daxa::Format::D32_SFLOAT,
            .size = {gpu_context.render_resolution.x, gpu_context.render_resolution.y, 1},
            .name = "depth_target",
        });
        gpu_context.add(RasterTask<R32D32Blit::Task, R32D32BlitPush, NoTaskInfo>{
            .vert_source = daxa::ShaderFile{"FULL_SCREEN_TRIANGLE_VERTEX_SHADER"},
            .frag_source = daxa::ShaderFile{"R32_D32_BLIT"},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::ALWAYS,
            },
            .views = std::array{
                daxa::TaskViewVariant{std::pair{R32D32Blit::AT.input_tex, temp_depth_image}},
                daxa::TaskViewVariant{std::pair{R32D32Blit::AT.output_tex, depth_target}},
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::RasterPipeline &pipeline, R32D32BlitPush &push, NoTaskInfo const &) {
                auto render_image = ti.get(R32D32Blit::AT.output_tex).ids[0];
                auto const image_info = ti.device.info_image(render_image).value();
                auto renderpass_recorder = std::move(ti.recorder).begin_renderpass({
                    .depth_attachment = {{.image_view = ti.get(R32D32Blit::AT.output_tex).view_ids[0], .load_op = daxa::AttachmentLoadOp::CLEAR, .clear_value = std::array{0.0f, 0.0f, 0.0f, 0.0f}}},
                    .render_area = {.x = 0, .y = 0, .width = image_info.size.x, .height = image_info.size.y},
                });
                renderpass_recorder.set_pipeline(pipeline);
                set_push_constant(ti, renderpass_recorder, push);
                renderpass_recorder.draw({.vertex_count = 3});
                ti.recorder = std::move(renderpass_recorder).end_renderpass();
            },
            .task_graph = &task_graph,
        });

        gpu_context.add(RasterTask<DebugLines::Task, DebugLinesPush, DebugShapeRenderer const *>{
            .vert_source = daxa::ShaderFile{"debug_shapes.glsl"},
            .frag_source = daxa::ShaderFile{"debug_shapes.glsl"},
            .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}},
            .depth_test = daxa::DepthTestInfo{.depth_attachment_format = daxa::Format::D32_SFLOAT, .depth_test_compare_op = daxa::CompareOp::GREATER},
            .raster = {.primitive_topology = daxa::PrimitiveTopology::LINE_LIST},
            .views = std::array{
                daxa::attachment_view(DebugLines::AT.gpu_input, gpu_context.task_input_buffer),
                daxa::attachment_view(DebugLines::AT.render_target, render_target),
                daxa::attachment_view(DebugLines::AT.depth_target, depth_target),
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::RasterPipeline &pipeline, DebugLinesPush &push, DebugShapeRenderer const *const &state) {
                if (state->lines.empty()) {
                    return;
                }

                auto const &image_attach_info = ti.get(DebugLines::AT.render_target);
                auto const &depth_attach_info = ti.get(DebugLines::AT.depth_target);
                auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
                auto render_recorder = std::move(ti.recorder).begin_renderpass({
                    .color_attachments = std::array{
                        daxa::RenderAttachmentInfo{
                            .image_view = image_attach_info.view_ids[0],
                            .load_op = daxa::AttachmentLoadOp::LOAD,
                        },
                    },
                    .depth_attachment = daxa::RenderAttachmentInfo{
                        .image_view = depth_attach_info.view_ids[0],
                        .load_op = daxa::AttachmentLoadOp::LOAD,
                    },
                    .render_area = {.width = image_info.size.x, .height = image_info.size.y},
                });
                render_recorder.set_pipeline(pipeline);

                auto size = state->lines.size() * sizeof(Line);
                auto lines_buffer = ti.device.create_buffer({
                    .size = size,
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
                    .name = "lines_buffer",
                });
                auto alloc_host_address = ti.device.get_host_address(lines_buffer).value();
                auto alloc_device_address = ti.device.get_device_address(lines_buffer).value();
                memcpy(alloc_host_address, state->lines.data(), size);
                push.vertex_data = alloc_device_address;
                push.flags = *state->draw_from_observer;

                set_push_constant(ti, render_recorder, push);
                render_recorder.draw({.vertex_count = uint32_t(2 * state->lines.size())});
                ti.recorder = std::move(render_recorder).end_renderpass();
                ti.recorder.destroy_buffer_deferred(lines_buffer);
            },
            .info = state,
            .task_graph = &task_graph,
        });

        gpu_context.add(RasterTask<DebugPoints::Task, DebugPointsPush, DebugShapeRenderer const *>{
            .vert_source = daxa::ShaderFile{"debug_shapes.glsl"},
            .frag_source = daxa::ShaderFile{"debug_shapes.glsl"},
            .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}},
            .depth_test = daxa::DepthTestInfo{.depth_attachment_format = daxa::Format::D32_SFLOAT, .depth_test_compare_op = daxa::CompareOp::GREATER},
            .raster = {.primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST},
            .views = std::array{
                daxa::attachment_view(DebugPoints::AT.render_target, render_target),
                daxa::attachment_view(DebugPoints::AT.gpu_input, gpu_context.task_input_buffer),
                daxa::attachment_view(DebugPoints::AT.depth_target, depth_target),
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::RasterPipeline &pipeline, DebugPointsPush &push, DebugShapeRenderer const *const &state) {
                if (state->points.empty()) {
                    return;
                }
                auto const &image_attach_info = ti.get(DebugPoints::AT.render_target);
                auto const &depth_attach_info = ti.get(DebugLines::AT.depth_target);
                auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
                auto render_recorder = std::move(ti.recorder).begin_renderpass({
                    .color_attachments = std::array{
                        daxa::RenderAttachmentInfo{
                            .image_view = image_attach_info.view_ids[0],
                            .load_op = daxa::AttachmentLoadOp::LOAD,
                        },
                    },
                    .depth_attachment = daxa::RenderAttachmentInfo{
                        .image_view = depth_attach_info.view_ids[0],
                        .load_op = daxa::AttachmentLoadOp::LOAD,
                    },
                    .render_area = {.width = image_info.size.x, .height = image_info.size.y},
                });
                render_recorder.set_pipeline(pipeline);

                auto size = state->points.size() * sizeof(Point);
                auto points_buffer = ti.device.create_buffer({
                    .size = size,
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
                    .name = "points_buffer",
                });
                auto alloc_host_address = ti.device.get_host_address(points_buffer).value();
                auto alloc_device_address = ti.device.get_device_address(points_buffer).value();
                memcpy(alloc_host_address, state->points.data(), size);
                push.vertex_data = alloc_device_address;
                push.flags = *state->draw_from_observer;

                set_push_constant(ti, render_recorder, push);
                render_recorder.draw({.vertex_count = uint32_t(6 * state->points.size())});
                ti.recorder = std::move(render_recorder).end_renderpass();
                ti.recorder.destroy_buffer_deferred(points_buffer);
            },
            .info = state,
            .task_graph = &task_graph,
        });
    }
} // namespace renderer

#endif
