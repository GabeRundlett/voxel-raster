#pragma once

#include <renderer/shared.inl>

DAXA_DECL_TASK_HEAD_BEGIN(TracePrimary)
DAXA_TH_BUFFER_PTR(RAY_TRACING_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(RAY_TRACING_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(RAY_TRACING_SHADER_READ, brick_data)
DAXA_TH_TLAS_PTR(RAY_TRACING_SHADER_READ, tlas)
DAXA_TH_IMAGE_ID(RAY_TRACING_SHADER_STORAGE_WRITE_ONLY, REGULAR_2D, color)
DAXA_TH_IMAGE_ID(RAY_TRACING_SHADER_STORAGE_WRITE_ONLY, REGULAR_2D, depth)
DAXA_TH_IMAGE_ID(RAY_TRACING_SHADER_STORAGE_WRITE_ONLY, REGULAR_2D, motion_vectors)
DAXA_DECL_TASK_HEAD_END

struct TracePrimaryPush {
    DAXA_TH_BLOB(TracePrimary, uses)
};

#if defined(__cplusplus)

#include <renderer/renderer.hpp>
#include <renderer/utilities/gpu_context.hpp>
#include <renderer/utilities/common.hpp>

namespace renderer {
    auto raytrace(GpuContext &gpu_context, daxa::TaskGraph &task_graph, daxa::TaskTlasView task_tlas, daxa::TaskBufferView task_chunks, daxa::TaskBufferView task_brick_data, uint32_t &chunk_n, bool &draw_from_observer) -> std::array<daxa::TaskImageView, 3> {
        auto color = task_graph.create_transient_image({
            .format = daxa::Format::R16G16B16A16_SFLOAT,
            .size = {gpu_context.render_resolution.x, gpu_context.render_resolution.y, 1},
            .name = "color",
        });
        auto depth = task_graph.create_transient_image({
            .format = daxa::Format::R32_SFLOAT,
            .size = {gpu_context.render_resolution.x, gpu_context.render_resolution.y, 1},
            .name = "depth",
        });
        auto motion_vectors = task_graph.create_transient_image({
            .format = daxa::Format::R16G16_SFLOAT,
            .size = {gpu_context.render_resolution.x, gpu_context.render_resolution.y, 1},
            .name = "motion_vectors",
        });

        gpu_context.add(RayTracingTask<TracePrimary::Task, TracePrimaryPush, NoTaskInfo>{
            .source = daxa::ShaderFile{"voxel_raytracer/trace_primary.glsl"},
            .views = std::array{
                daxa::TaskViewVariant{std::pair{TracePrimary::AT.gpu_input, gpu_context.task_input_buffer}},
                daxa::TaskViewVariant{std::pair{TracePrimary::AT.chunks, task_chunks}},
                daxa::TaskViewVariant{std::pair{TracePrimary::AT.brick_data, task_brick_data}},
                daxa::TaskViewVariant{std::pair{TracePrimary::AT.tlas, task_tlas}},
                daxa::TaskViewVariant{std::pair{TracePrimary::AT.color, color}},
                daxa::TaskViewVariant{std::pair{TracePrimary::AT.depth, depth}},
                daxa::TaskViewVariant{std::pair{TracePrimary::AT.motion_vectors, motion_vectors}},
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::RayTracingPipeline &pipeline, TracePrimaryPush &push, NoTaskInfo const &) {
                auto const image_info = ti.device.info_image(ti.get(TracePrimary::AT.color).ids[0]).value();
                ti.recorder.set_pipeline(pipeline);
                set_push_constant(ti, push);
                ti.recorder.trace_rays({.width = image_info.size.x, .height = image_info.size.y, .depth = 1});
            },
            .task_graph = &task_graph,
        });

        return {color, depth, motion_vectors};
    }
} // namespace renderer

#endif
