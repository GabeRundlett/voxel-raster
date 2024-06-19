#pragma once

#include <renderer/shared.inl>

DAXA_DECL_TASK_HEAD_BEGIN(DebugLines)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
DAXA_DECL_TASK_HEAD_END

struct DebugLinesPush {
    DAXA_TH_BLOB(DebugLines, uses)
    daxa_BufferPtr(daxa_f32vec3) vertex_data;
    daxa_u32 flags;
};

DAXA_DECL_TASK_HEAD_BEGIN(DebugPoints)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
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

    inline void draw_debug_shapes(GpuContext &gpu_context, daxa::TaskGraph &task_graph, daxa::TaskImageView render_target, DebugShapeRenderer const *state) {
        gpu_context.add(RasterTask<DebugLines::Task, DebugLinesPush, DebugShapeRenderer const *>{
            .vert_source = daxa::ShaderFile{"debug_shapes.glsl"},
            .frag_source = daxa::ShaderFile{"debug_shapes.glsl"},
            .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}},
            .raster = {.primitive_topology = daxa::PrimitiveTopology::LINE_LIST},
            .views = std::array{
                daxa::attachment_view(DebugLines::AT.render_target, render_target),
                daxa::attachment_view(DebugLines::AT.gpu_input, gpu_context.task_input_buffer),
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::RasterPipeline &pipeline, DebugLinesPush &push, DebugShapeRenderer const *const &state) {
                if (state->lines.empty()) {
                    return;
                }

                auto const &image_attach_info = ti.get(DebugLines::AT.render_target);
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
                render_recorder.set_pipeline(pipeline);

                auto size = state->lines.size() * sizeof(Line);
                auto alloc = ti.allocator->allocate(size).value();
                memcpy(alloc.host_address, state->lines.data(), size);
                push.vertex_data = alloc.device_address;
                push.flags = *state->draw_from_observer;

                render_recorder.push_constant(push);
                render_recorder.draw({.vertex_count = uint32_t(2 * state->lines.size())});
                ti.recorder = std::move(render_recorder).end_renderpass();
            },
            .info = state,
            .task_graph = &task_graph,
        });

        gpu_context.add(RasterTask<DebugPoints::Task, DebugPointsPush, DebugShapeRenderer const *>{
            .vert_source = daxa::ShaderFile{"debug_shapes.glsl"},
            .frag_source = daxa::ShaderFile{"debug_shapes.glsl"},
            .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}},
            .raster = {.primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST},
            .views = std::array{
                daxa::attachment_view(DebugPoints::AT.render_target, render_target),
                daxa::attachment_view(DebugPoints::AT.gpu_input, gpu_context.task_input_buffer),
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::RasterPipeline &pipeline, DebugPointsPush &push, DebugShapeRenderer const *const &state) {
                if (state->points.empty()) {
                    return;
                }
                auto const &image_attach_info = ti.get(DebugPoints::AT.render_target);
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
                render_recorder.set_pipeline(pipeline);

                auto size = state->points.size() * sizeof(Point);
                auto alloc = ti.allocator->allocate(size).value();
                memcpy(alloc.host_address, state->points.data(), size);
                push.vertex_data = alloc.device_address;
                push.flags = *state->draw_from_observer;

                render_recorder.push_constant(push);
                render_recorder.draw({.vertex_count = uint32_t(6 * state->points.size())});
                ti.recorder = std::move(render_recorder).end_renderpass();
            },
            .info = state,
            .task_graph = &task_graph,
        });
    }
} // namespace renderer

#endif
