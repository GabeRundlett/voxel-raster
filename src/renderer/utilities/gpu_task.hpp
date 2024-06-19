#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_graph.hpp>
#include "async_pipeline_manager.hpp"

template <typename TaskHeadT, typename PushT, typename InfoT, typename PipelineT>
using TaskCallback = void(daxa::TaskInterface const &ti, typename PipelineT::PipelineT &pipeline, PushT &push, InfoT const &info);

struct NoTaskInfo {
};

template <typename TaskHeadT, typename PushT, typename InfoT, typename PipelineT>
struct Task : TaskHeadT {
    daxa::ShaderSource source;
    std::optional<uint32_t> required_subgroup_size{};
    std::vector<daxa::ShaderDefine> extra_defines{};
    TaskHeadT::AttachmentViews views{};
    TaskCallback<TaskHeadT, PushT, InfoT, PipelineT> *callback_{};
    InfoT info{};
    // Not set by user
    std::shared_ptr<PipelineT> pipeline;
    daxa::TaskGraph *task_graph = nullptr;
    void callback(daxa::TaskInterface const &ti) {
        auto push = PushT{};
        if (!pipeline->is_valid()) {
            return;
        }
        callback_(ti, pipeline->get(), push, info);
    }
};

template <typename TaskHeadT, typename PushT, typename InfoT>
struct Task<TaskHeadT, PushT, InfoT, AsyncManagedRayTracingPipeline> : TaskHeadT {
    daxa::ShaderSource source;
    uint32_t max_ray_recursion_depth = 1;
    std::vector<daxa::ShaderDefine> extra_defines{};
    TaskHeadT::AttachmentViews views{};
    TaskCallback<TaskHeadT, PushT, InfoT, AsyncManagedRayTracingPipeline> *callback_{};
    InfoT info{};
    // Not set by user
    std::shared_ptr<AsyncManagedRayTracingPipeline> pipeline;
    daxa::TaskGraph *task_graph = nullptr;
    void callback(daxa::TaskInterface const &ti) {
        auto push = PushT{};
        if (!pipeline->is_valid()) {
            return;
        }
        callback_(ti, pipeline->get(), push, info);
    }
};

template <typename TaskHeadT, typename PushT, typename InfoT>
struct Task<TaskHeadT, PushT, InfoT, AsyncManagedRasterPipeline> : TaskHeadT {
    daxa::ShaderSource vert_source{};
    daxa::ShaderSource mesh_source{};
    daxa::ShaderSource frag_source{};
    std::vector<daxa::RenderAttachment> color_attachments{};
    daxa::Optional<daxa::DepthTestInfo> depth_test{};
    daxa::RasterizerInfo raster{};
    std::optional<uint32_t> required_subgroup_size{};
    std::vector<daxa::ShaderDefine> extra_defines{};
    TaskHeadT::AttachmentViews views{};
    TaskCallback<TaskHeadT, PushT, InfoT, AsyncManagedRasterPipeline> *callback_{};
    InfoT info{};
    // Not set by user
    std::shared_ptr<AsyncManagedRasterPipeline> pipeline;
    daxa::TaskGraph *task_graph = nullptr;
    void callback(daxa::TaskInterface const &ti) {
        auto push = PushT{};
        if (!pipeline->is_valid()) {
            return;
        }
        callback_(ti, pipeline->get(), push, info);
    }
};

template <typename TaskHeadT, typename PushT, typename InfoT>
using ComputeTask = Task<TaskHeadT, PushT, InfoT, AsyncManagedComputePipeline>;

template <typename TaskHeadT, typename PushT, typename InfoT>
using RayTracingTask = Task<TaskHeadT, PushT, InfoT, AsyncManagedRayTracingPipeline>;

template <typename TaskHeadT, typename PushT, typename InfoT>
using RasterTask = Task<TaskHeadT, PushT, InfoT, AsyncManagedRasterPipeline>;

namespace {
    template <typename PushT>
    constexpr auto push_constant_size() -> uint32_t {
        return static_cast<uint32_t>(((sizeof(PushT) & ~0x3u) + 7u) & ~7u);
    }

    template <typename PushT>
    void set_push_constant(daxa::TaskInterface const &ti, PushT push) {
        if constexpr (requires(PushT p) { p.uses; }) {
            ti.assign_attachment_shader_blob(push.uses.value);
        }
        ti.recorder.push_constant(push);
    }

    template <typename PushT>
    void set_push_constant(daxa::TaskInterface const &ti, daxa::RenderCommandRecorder &render_recorder, PushT push) {
        if constexpr (requires(PushT p) { p.uses; }) {
            ti.assign_attachment_shader_blob(push.uses.value);
        }
        render_recorder.push_constant(push);
    }
} // namespace
