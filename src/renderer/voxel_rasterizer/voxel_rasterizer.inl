#pragma once

#include <renderer/shared.inl>

DAXA_DECL_TASK_HEAD_BEGIN(ClearDrawFlags)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ_WRITE, brick_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ, indirect_info)
DAXA_DECL_TASK_HEAD_END

struct ClearDrawFlagsPush {
    DAXA_TH_BLOB(ClearDrawFlags, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(AllocateBrickInstances)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ, brick_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_SAMPLED, REGULAR_2D, hiz)
DAXA_DECL_TASK_HEAD_END

struct AllocateBrickInstancesPush {
    DAXA_TH_BLOB(AllocateBrickInstances, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(SetIndirectInfos)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstanceAllocatorState), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelMeshletAllocatorState), meshlet_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(DispatchIndirectStruct), indirect_info)
DAXA_DECL_TASK_HEAD_END

struct SetIndirectInfosPush {
    DAXA_TH_BLOB(SetIndirectInfos, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(MeshVoxelBricks)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ_WRITE, brick_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(VoxelMeshletMetadata), meshlet_metadata)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), indirect_info)
DAXA_DECL_TASK_HEAD_END

struct MeshVoxelBricksPush {
    DAXA_TH_BLOB(MeshVoxelBricks, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(DrawVisbuffer)
DAXA_TH_IMAGE_INDEX(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, REGULAR_2D, visbuffer64)
// DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
// DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(MESH_SHADER_READ, brick_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(VoxelMeshletMetadata), meshlet_metadata)
DAXA_TH_BUFFER_PTR(DRAW_INDIRECT_INFO_READ, daxa_BufferPtr(DispatchIndirectStruct), indirect_info)
#if ENABLE_DEBUG_VIS
DAXA_TH_IMAGE_INDEX(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, REGULAR_2D, debug_overdraw)
#endif
DAXA_TH_IMAGE_INDEX(MESH_SHADER_SAMPLED, REGULAR_2D, hiz)
DAXA_DECL_TASK_HEAD_END

struct DrawVisbufferPush {
    DAXA_TH_BLOB(DrawVisbuffer, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(ComputeRasterize)
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_STORAGE_READ_WRITE, REGULAR_2D, visbuffer64)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ, brick_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelMeshletMetadata), meshlet_metadata)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), indirect_info)
#if ENABLE_DEBUG_VIS
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_STORAGE_READ_WRITE_CONCURRENT, REGULAR_2D, debug_overdraw)
#endif
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_SAMPLED, REGULAR_2D, hiz)
DAXA_DECL_TASK_HEAD_END

struct ComputeRasterizePush {
    DAXA_TH_BLOB(ComputeRasterize, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(ShadeVisbuffer)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ, brick_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelMeshletMetadata), meshlet_metadata)
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_STORAGE_READ_ONLY, REGULAR_2D, visbuffer64)
#if ENABLE_DEBUG_VIS
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_SAMPLED, REGULAR_2D, debug_overdraw)
#endif
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_STORAGE_WRITE_ONLY, REGULAR_2D, color)
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_STORAGE_WRITE_ONLY, REGULAR_2D, depth)
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_STORAGE_WRITE_ONLY, REGULAR_2D, motion_vectors)
DAXA_DECL_TASK_HEAD_END

struct ShadeVisbufferPush {
    DAXA_TH_BLOB(ShadeVisbuffer, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(AnalyzeVisbuffer)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ_WRITE, brick_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelMeshletMetadata), meshlet_metadata)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(daxa_u32), brick_visibility_bits)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(BrickInstance), visible_brick_instance_allocator)
// DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_SAMPLED, REGULAR_2D, visbuffer)
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_STORAGE_READ_ONLY, REGULAR_2D, visbuffer64)
DAXA_DECL_TASK_HEAD_END

struct AnalyzeVisbufferPush {
    DAXA_TH_BLOB(AnalyzeVisbuffer, uses)
};

#define GEN_HIZ_X 16
#define GEN_HIZ_Y 16
#define GEN_HIZ_LEVELS_PER_DISPATCH 12
#define GEN_HIZ_WINDOW_X 64
#define GEN_HIZ_WINDOW_Y 64
DAXA_DECL_TASK_HEAD_BEGIN(GenHiz)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_STORAGE_READ_ONLY, REGULAR_2D, src)
DAXA_TH_IMAGE_ID_MIP_ARRAY(COMPUTE_SHADER_STORAGE_READ_WRITE, REGULAR_2D, mips, GEN_HIZ_LEVELS_PER_DISPATCH)
DAXA_DECL_TASK_HEAD_END

struct GenHizPush {
    DAXA_TH_BLOB(GenHiz, uses)
    daxa_RWBufferPtr(daxa_u32) counter;
    daxa_u32 mip_count;
    daxa_u32 total_workgroup_count;
};

#if defined(__cplusplus)

#include <renderer/renderer.hpp>
#include <renderer/utilities/gpu_context.hpp>
#include <renderer/utilities/common.hpp>

struct AllocateBrickInstancesTask : AllocateBrickInstances::Task {
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

struct SetIndirectInfosTask : SetIndirectInfos::Task {
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

struct MeshVoxelBricksTask : MeshVoxelBricks::Task {
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

struct DrawVisbufferTask : DrawVisbuffer::Task {
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

struct ComputeRasterizeTask : ComputeRasterize::Task {
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

struct ShadeVisbufferTask : ShadeVisbuffer::Task {
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

struct PostProcessingTask : PostProcessing::Task {
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

struct AnalyzeVisbufferTask : AnalyzeVisbuffer::Task {
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

struct GenHizTask : GenHiz::Task {
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

struct ClearDrawFlagsTask : ClearDrawFlags::Task {
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

auto task_gen_hiz_single_pass(GpuContext &gpu_context, daxa::TaskGraph &task_graph, daxa::TaskBufferView gpu_input, daxa::TaskImageView task_visbuffer64, std::shared_ptr<daxa::ComputePipeline> &gen_hiz_pipeline) -> daxa::TaskImageView {
    auto const x_ = gpu_context.next_lower_po2_render_size.x;
    auto const y_ = gpu_context.next_lower_po2_render_size.y;
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
            daxa::attachment_view(GenHiz::AT.gpu_input, gpu_input),
            daxa::attachment_view(GenHiz::AT.src, task_visbuffer64),
            daxa::attachment_view(GenHiz::AT.mips, task_hiz),
        },
        .pipeline = gen_hiz_pipeline.get(),
    });
    return task_hiz;
}

namespace renderer {
    struct VoxelRasterizer {
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
        std::shared_ptr<daxa::ComputePipeline> shade_visbuffer_pipeline;
        std::shared_ptr<daxa::ComputePipeline> analyze_visbuffer_pipeline;
        std::shared_ptr<daxa::ComputePipeline> gen_hiz_pipeline;

        daxa::BufferId brick_meshlet_allocator;
        daxa::BufferId brick_meshlet_metadata;
        daxa::BufferId brick_instance_allocator;

        daxa::TaskBuffer task_brick_meshlet_allocator;
        daxa::TaskBuffer task_brick_meshlet_metadata;
        daxa::TaskBuffer task_brick_instance_allocator;

        daxa::BufferId brick_visibility_bits;
        daxa::TaskBuffer task_brick_visibility_bits;

        daxa::BufferId visible_brick_instance_allocator;
        daxa::TaskBuffer task_visible_brick_instance_allocator;
    };

    void init(VoxelRasterizer *self, daxa::Device &device, daxa::PipelineManager &pipeline_manager) {
        {
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/clear_draw_flags.glsl"}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/allocate_brick_instances.glsl"}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/set_indirect_infos.glsl"}, .compile_options = {.defines = {{"SET_TYPE", "0"}}}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/set_indirect_infos.glsl"}, .compile_options = {.defines = {{"SET_TYPE", "1"}}}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/set_indirect_infos.glsl"}, .compile_options = {.defines = {{"SET_TYPE", "2"}}}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/mesh_voxel_bricks.glsl"}},
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
            auto result = pipeline_manager.add_raster_pipeline({
                .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"}, .compile_options = {.required_subgroup_size = 32}},
                .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"}},
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
            auto result = pipeline_manager.add_raster_pipeline({
                .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"}, .compile_options = {.defines = {{"DO_DEPTH_CULL", "1"}}, .required_subgroup_size = 32}},
                .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"}},
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
            auto result = pipeline_manager.add_raster_pipeline({
                .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"}, .compile_options = {.defines = {{"DRAW_FROM_OBSERVER", "1"}}, .required_subgroup_size = 32}},
                .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"}},
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
            auto result = pipeline_manager.add_raster_pipeline({
                .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"}, .compile_options = {.defines = {{"DRAW_FROM_OBSERVER", "1"}, {"DO_DEPTH_CULL", "1"}}, .required_subgroup_size = 32}},
                .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/compute_rasterize.glsl"}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/compute_rasterize.glsl"}, .compile_options = {.defines = {{"DO_DEPTH_CULL", "1"}}}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/compute_rasterize.glsl"}, .compile_options = {.defines = {{"DRAW_FROM_OBSERVER", "1"}}}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/compute_rasterize.glsl"}, .compile_options = {.defines = {{"DRAW_FROM_OBSERVER", "1"}, {"DO_DEPTH_CULL", "1"}}}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/shade_visbuffer.glsl"}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/analyze_visbuffer.glsl"}},
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
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxel_rasterizer/gen_hiz.glsl"}},
                .push_constant_size = sizeof(GenHizPush),
                .name = "gen_hiz",
            });
            if (result.is_err()) {
                std::cerr << result.message() << std::endl;
                std::terminate();
            }
            self->gen_hiz_pipeline = result.value();
        }

        self->task_brick_meshlet_allocator = daxa::TaskBuffer({.name = "task_brick_meshlet_allocator"});
        self->task_brick_meshlet_metadata = daxa::TaskBuffer({.name = "task_brick_meshlet_metadata"});
        self->task_brick_instance_allocator = daxa::TaskBuffer({.name = "task_brick_instance_allocator"});
        self->task_brick_visibility_bits = daxa::TaskBuffer({.name = "task_brick_visibility_bits"});
        self->task_visible_brick_instance_allocator = daxa::TaskBuffer({.name = "task_visible_brick_instance_allocator"});

        self->brick_meshlet_allocator = device.create_buffer({
            // + 1 for the state at index 0
            .size = sizeof(VoxelMeshlet) * (MAX_MESHLET_COUNT + 1),
            .name = "brick_meshlet_allocator",
        });
        self->brick_meshlet_metadata = device.create_buffer({
            // + 1 for the state at index 0
            .size = sizeof(VoxelMeshletMetadata) * (MAX_MESHLET_COUNT + 1),
            .name = "brick_meshlet_metadata",
        });
        self->brick_instance_allocator = device.create_buffer({
            // + 1 for the state at index 0
            .size = sizeof(BrickInstance) * (MAX_BRICK_INSTANCE_COUNT + 1),
            .name = "brick_instance_allocator",
        });
        self->task_brick_meshlet_allocator.set_buffers({.buffers = std::array{self->brick_meshlet_allocator}});
        self->task_brick_meshlet_metadata.set_buffers({.buffers = std::array{self->brick_meshlet_metadata}});
        self->task_brick_instance_allocator.set_buffers({.buffers = std::array{self->brick_instance_allocator}});

        self->brick_visibility_bits = device.create_buffer({
            // + 1 for the state at index 0
            .size = sizeof(uint32_t) * (MAX_BRICK_INSTANCE_COUNT + 1),
            .name = "brick_visibility_bits",
        });
        self->task_brick_visibility_bits.set_buffers({.buffers = std::array{self->brick_visibility_bits}});
    }

    void deinit(VoxelRasterizer *self, daxa::Device &device) {
        if (!self->brick_meshlet_allocator.is_empty()) {
            device.destroy_buffer(self->brick_meshlet_allocator);
        }
        if (!self->brick_meshlet_metadata.is_empty()) {
            device.destroy_buffer(self->brick_meshlet_metadata);
        }
        if (!self->brick_instance_allocator.is_empty()) {
            device.destroy_buffer(self->brick_instance_allocator);
        }

        if (!self->brick_visibility_bits.is_empty()) {
            device.destroy_buffer(self->brick_visibility_bits);
        }
        if (!self->visible_brick_instance_allocator.is_empty()) {
            device.destroy_buffer(self->visible_brick_instance_allocator);
        }
    }

    void on_resize(VoxelRasterizer *self, daxa::Device &device, int size_x, int size_y) {
        if (!self->visible_brick_instance_allocator.is_empty()) {
            device.destroy_buffer(self->visible_brick_instance_allocator);
        }
        self->visible_brick_instance_allocator = device.create_buffer({
            // + 1 for the state at index 0
            .size = sizeof(BrickInstance) * (size_x * size_y + 1),
            .name = "visible_brick_instance_allocator",
        });
        self->task_visible_brick_instance_allocator.set_buffers({.buffers = std::array{self->visible_brick_instance_allocator}});

        auto temp_task_graph = daxa::TaskGraph({
            .device = device,
            .name = "temp_task_graph",
        });
        temp_task_graph.use_persistent_buffer(self->task_visible_brick_instance_allocator);
        task_fill_buffer(temp_task_graph, self->task_visible_brick_instance_allocator, uint32_t{0});
        temp_task_graph.submit({});
        temp_task_graph.complete({});
        temp_task_graph.execute({});
    }

    void update(VoxelRasterizer *self, daxa::Device &device, daxa::TaskBuffer &task_chunks, daxa::TaskBuffer &task_brick_data) {
        auto temp_task_graph = daxa::TaskGraph({
            .device = device,
            .name = "update_task_graph",
        });
        temp_task_graph.use_persistent_buffer(task_brick_data);
        temp_task_graph.use_persistent_buffer(task_chunks);

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
                daxa::attachment_view(SetIndirectInfos::AT.gpu_input, task_input_buffer),
                daxa::attachment_view(SetIndirectInfos::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(SetIndirectInfos::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(SetIndirectInfos::AT.indirect_info, task_indirect_infos),
            },
            .pipeline = self->set_indirect_infos0.get(),
        });
        temp_task_graph.add_task(ClearDrawFlagsTask{
            .views = std::array{
                daxa::attachment_view(ClearDrawFlags::AT.chunks, task_chunks),
                daxa::attachment_view(ClearDrawFlags::AT.brick_data, task_brick_data),
                daxa::attachment_view(ClearDrawFlags::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(ClearDrawFlags::AT.indirect_info, task_indirect_infos),
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

    auto render(VoxelRasterizer *self, GpuContext &gpu_context, daxa::TaskGraph &task_graph, daxa::TaskBufferView task_chunks, daxa::TaskBufferView task_brick_data, uint32_t &chunk_n, bool &draw_from_observer) -> std::array<daxa::TaskImageView, 3> {
        task_graph.use_persistent_buffer(self->task_brick_meshlet_allocator);
        task_graph.use_persistent_buffer(self->task_brick_meshlet_metadata);
        task_graph.use_persistent_buffer(self->task_brick_instance_allocator);
        task_graph.use_persistent_buffer(self->task_brick_visibility_bits);
        task_graph.use_persistent_buffer(self->task_visible_brick_instance_allocator);

#if ENABLE_DEBUG_VIS
        auto task_debug_overdraw = task_graph.create_transient_image({
            .format = daxa::Format::R32_UINT,
            .size = {gpu_context.render_resolution.x, gpu_context.render_resolution.y, 1},
            .name = "debug_overdraw",
        });
        clear_task_images(task_graph, std::array<daxa::TaskImageView, 1>{task_debug_overdraw}, std::array<daxa::ClearValue, 1>{std::array<uint32_t, 4>{0, 0, 0, 0}});
#endif

        auto task_indirect_infos = task_graph.create_transient_buffer({
            .size = sizeof(DispatchIndirectStruct) * 4,
            .name = "indirect_infos",
        });

        auto task_visbuffer64 = task_graph.create_transient_image({
            .format = daxa::Format::R64_UINT,
            .size = {gpu_context.render_resolution.x, gpu_context.render_resolution.y, 1},
            .name = "visbuffer64",
        });
        clear_task_images(task_graph, std::array<daxa::TaskImageView, 1>{task_visbuffer64}, std::array<daxa::ClearValue, 1>{std::array<uint32_t, 4>{0, 0, 0, 0}});

        auto task_observer_visbuffer64 = daxa::TaskImageView{};
        if (draw_from_observer) {
            task_observer_visbuffer64 = task_graph.create_transient_image({
                .format = daxa::Format::R64_UINT,
                .size = {gpu_context.render_resolution.x, gpu_context.render_resolution.y, 1},
                .name = "observer_visbuffer64",
            });
            clear_task_images(task_graph, std::array<daxa::TaskImageView, 1>{task_observer_visbuffer64}, std::array<daxa::ClearValue, 1>{std::array<uint32_t, 4>{0, 0, 0, 0}});
        }

        auto draw_brick_instances = [&, first_draw = true](daxa::TaskBufferView task_brick_instance_allocator, daxa::TaskImageView hiz) mutable {
            if (first_draw) {
                task_fill_buffer(task_graph, self->task_brick_meshlet_allocator, daxa_u32vec2{});
            }

            task_graph.add_task(SetIndirectInfosTask{
                .views = std::array{
                    daxa::attachment_view(SetIndirectInfos::AT.gpu_input, gpu_context.task_input_buffer),
                    daxa::attachment_view(SetIndirectInfos::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(SetIndirectInfos::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(SetIndirectInfos::AT.indirect_info, task_indirect_infos),
                },
                .pipeline = first_draw ? self->set_indirect_infos0.get() : self->set_indirect_infos1.get(),
            });

            task_graph.add_task(MeshVoxelBricksTask{
                .views = std::array{
                    daxa::attachment_view(MeshVoxelBricks::AT.gpu_input, gpu_context.task_input_buffer),
                    daxa::attachment_view(MeshVoxelBricks::AT.chunks, task_chunks),
                    daxa::attachment_view(MeshVoxelBricks::AT.brick_data, task_brick_data),
                    daxa::attachment_view(MeshVoxelBricks::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(MeshVoxelBricks::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(MeshVoxelBricks::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
                    daxa::attachment_view(MeshVoxelBricks::AT.indirect_info, task_indirect_infos),
                },
                .pipeline = self->mesh_voxel_bricks_pipeline.get(),
                .indirect_offset = sizeof(DispatchIndirectStruct) * 0,
            });

            task_graph.add_task(SetIndirectInfosTask{
                .views = std::array{
                    daxa::attachment_view(SetIndirectInfos::AT.gpu_input, gpu_context.task_input_buffer),
                    daxa::attachment_view(SetIndirectInfos::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(SetIndirectInfos::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(SetIndirectInfos::AT.indirect_info, task_indirect_infos),
                },
                .pipeline = self->set_indirect_infos2.get(),
            });

            task_graph.add_task(DrawVisbufferTask{
                .views = std::array{
                    daxa::attachment_view(DrawVisbuffer::AT.visbuffer64, task_visbuffer64),
                    daxa::attachment_view(DrawVisbuffer::AT.gpu_input, gpu_context.task_input_buffer),
                    daxa::attachment_view(DrawVisbuffer::AT.chunks, task_chunks),
                    daxa::attachment_view(DrawVisbuffer::AT.brick_data, task_brick_data),
                    daxa::attachment_view(DrawVisbuffer::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(DrawVisbuffer::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(DrawVisbuffer::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
#if ENABLE_DEBUG_VIS
                    daxa::attachment_view(DrawVisbuffer::AT.debug_overdraw, task_debug_overdraw),
#endif
                    daxa::attachment_view(DrawVisbuffer::AT.indirect_info, task_indirect_infos),
                    daxa::attachment_view(DrawVisbuffer::AT.hiz, hiz),
                },
                .pipeline = hiz == daxa::NullTaskImage ? self->draw_visbuffer_pipeline.get() : self->draw_visbuffer_depth_cull_pipeline.get(),
                .indirect_offset = sizeof(DispatchIndirectStruct) * 3,
                .first = first_draw,
            });

            task_graph.add_task(ComputeRasterizeTask{
                .views = std::array{
                    daxa::attachment_view(ComputeRasterize::AT.visbuffer64, task_visbuffer64),
                    daxa::attachment_view(ComputeRasterize::AT.gpu_input, gpu_context.task_input_buffer),
                    daxa::attachment_view(ComputeRasterize::AT.chunks, task_chunks),
                    daxa::attachment_view(ComputeRasterize::AT.brick_data, task_brick_data),
                    daxa::attachment_view(ComputeRasterize::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(ComputeRasterize::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(ComputeRasterize::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
                    daxa::attachment_view(ComputeRasterize::AT.indirect_info, task_indirect_infos),
#if ENABLE_DEBUG_VIS
                    daxa::attachment_view(ComputeRasterize::AT.debug_overdraw, task_debug_overdraw),
#endif
                    daxa::attachment_view(ComputeRasterize::AT.hiz, hiz),
                },
                .pipeline = hiz == daxa::NullTaskImage ? self->compute_rasterize_pipeline.get() : self->compute_rasterize_depth_cull_pipeline.get(),
                .indirect_offset = sizeof(DispatchIndirectStruct) * 1,
            });

            if (draw_from_observer) {
                task_graph.add_task(DrawVisbufferTask{
                    .views = std::array{
                        daxa::attachment_view(DrawVisbuffer::AT.visbuffer64, task_observer_visbuffer64),
                        daxa::attachment_view(DrawVisbuffer::AT.gpu_input, gpu_context.task_input_buffer),
                        daxa::attachment_view(DrawVisbuffer::AT.chunks, task_chunks),
                        daxa::attachment_view(DrawVisbuffer::AT.brick_data, task_brick_data),
                        daxa::attachment_view(DrawVisbuffer::AT.brick_instance_allocator, task_brick_instance_allocator),
                        daxa::attachment_view(DrawVisbuffer::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                        daxa::attachment_view(DrawVisbuffer::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
#if ENABLE_DEBUG_VIS
                        daxa::attachment_view(DrawVisbuffer::AT.debug_overdraw, task_debug_overdraw),
#endif
                        daxa::attachment_view(DrawVisbuffer::AT.indirect_info, task_indirect_infos),
                        daxa::attachment_view(DrawVisbuffer::AT.hiz, hiz),
                    },
                    .pipeline = hiz == daxa::NullTaskImage ? self->draw_visbuffer_observer_pipeline.get() : self->draw_visbuffer_observer_depth_cull_pipeline.get(),
                    .indirect_offset = sizeof(DispatchIndirectStruct) * 3,
                    .first = first_draw,
                });

                task_graph.add_task(ComputeRasterizeTask{
                    .views = std::array{
                        daxa::attachment_view(ComputeRasterize::AT.visbuffer64, task_observer_visbuffer64),
                        daxa::attachment_view(ComputeRasterize::AT.gpu_input, gpu_context.task_input_buffer),
                        daxa::attachment_view(ComputeRasterize::AT.chunks, task_chunks),
                        daxa::attachment_view(ComputeRasterize::AT.brick_data, task_brick_data),
                        daxa::attachment_view(ComputeRasterize::AT.brick_instance_allocator, task_brick_instance_allocator),
                        daxa::attachment_view(ComputeRasterize::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                        daxa::attachment_view(ComputeRasterize::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
                        daxa::attachment_view(ComputeRasterize::AT.indirect_info, task_indirect_infos),
#if ENABLE_DEBUG_VIS
                        daxa::attachment_view(ComputeRasterize::AT.debug_overdraw, task_debug_overdraw),
#endif
                        daxa::attachment_view(ComputeRasterize::AT.hiz, hiz),
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
        auto hiz = task_gen_hiz_single_pass(gpu_context, task_graph, gpu_context.task_input_buffer, task_visbuffer64, self->gen_hiz_pipeline);

        // cull and draw the rest
        task_graph.add_task(AllocateBrickInstancesTask{
            .views = std::array{
                daxa::attachment_view(AllocateBrickInstances::AT.gpu_input, gpu_context.task_input_buffer),
                daxa::attachment_view(AllocateBrickInstances::AT.chunks, task_chunks),
                daxa::attachment_view(AllocateBrickInstances::AT.brick_data, task_brick_data),
                daxa::attachment_view(AllocateBrickInstances::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(AllocateBrickInstances::AT.hiz, hiz),
            },
            .pipeline = self->allocate_brick_instances_pipeline.get(),
            .chunk_n = &chunk_n,
        });
        draw_brick_instances(self->task_brick_instance_allocator, hiz);

        // clear draw flags. This needs to be done before the Analyze visbuffer pass,
        // since AnalyzeVisbuffer populates them again.
        task_graph.add_task(SetIndirectInfosTask{
            .views = std::array{
                daxa::attachment_view(SetIndirectInfos::AT.gpu_input, gpu_context.task_input_buffer),
                daxa::attachment_view(SetIndirectInfos::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(SetIndirectInfos::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(SetIndirectInfos::AT.indirect_info, task_indirect_infos),
            },
            .pipeline = self->set_indirect_infos0.get(),
        });
        task_graph.add_task(ClearDrawFlagsTask{
            .views = std::array{
                daxa::attachment_view(ClearDrawFlags::AT.chunks, task_chunks),
                daxa::attachment_view(ClearDrawFlags::AT.brick_data, task_brick_data),
                daxa::attachment_view(ClearDrawFlags::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(ClearDrawFlags::AT.indirect_info, task_indirect_infos),
            },
            .pipeline = self->clear_draw_flags_pipeline.get(),
        });

        task_fill_buffer(task_graph, self->task_visible_brick_instance_allocator, uint32_t{0});
        clear_task_buffers(task_graph, std::array<daxa::TaskBufferView, 1>{self->task_brick_visibility_bits}, std::array{daxa::BufferClearInfo{.size = sizeof(uint32_t) * (MAX_BRICK_INSTANCE_COUNT + 1)}});
        task_graph.add_task(AnalyzeVisbufferTask{
            .views = std::array{
                daxa::attachment_view(AnalyzeVisbuffer::AT.gpu_input, gpu_context.task_input_buffer),
                daxa::attachment_view(AnalyzeVisbuffer::AT.chunks, task_chunks),
                daxa::attachment_view(AnalyzeVisbuffer::AT.brick_data, task_brick_data),
                daxa::attachment_view(AnalyzeVisbuffer::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(AnalyzeVisbuffer::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(AnalyzeVisbuffer::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
                daxa::attachment_view(AnalyzeVisbuffer::AT.brick_visibility_bits, self->task_brick_visibility_bits),
                daxa::attachment_view(AnalyzeVisbuffer::AT.visible_brick_instance_allocator, self->task_visible_brick_instance_allocator),
                daxa::attachment_view(AnalyzeVisbuffer::AT.visbuffer64, task_visbuffer64),
            },
            .pipeline = self->analyze_visbuffer_pipeline.get(),
        });

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

        task_graph.add_task(ShadeVisbufferTask{
            .views = std::array{
                daxa::attachment_view(ShadeVisbuffer::AT.gpu_input, gpu_context.task_input_buffer),
                daxa::attachment_view(ShadeVisbuffer::AT.chunks, task_chunks),
                daxa::attachment_view(ShadeVisbuffer::AT.brick_data, task_brick_data),
                daxa::attachment_view(ShadeVisbuffer::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(ShadeVisbuffer::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(ShadeVisbuffer::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
                daxa::attachment_view(ShadeVisbuffer::AT.visbuffer64, draw_from_observer ? task_observer_visbuffer64 : task_visbuffer64),
#if ENABLE_DEBUG_VIS
                daxa::attachment_view(ShadeVisbuffer::AT.debug_overdraw, task_debug_overdraw),
#endif
                daxa::attachment_view(ShadeVisbuffer::AT.color, color),
                daxa::attachment_view(ShadeVisbuffer::AT.depth, depth),
                daxa::attachment_view(ShadeVisbuffer::AT.motion_vectors, motion_vectors),
            },
            .pipeline = self->shade_visbuffer_pipeline.get(),
        });

        task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_READ, self->task_visible_brick_instance_allocator),
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, self->task_brick_instance_allocator),
            },
            .task = [&gpu_context](daxa::TaskInterface const &ti) {
                ti.recorder.copy_buffer_to_buffer({
                    .src_buffer = ti.get(daxa::TaskBufferAttachmentIndex{0}).ids[0],
                    .dst_buffer = ti.get(daxa::TaskBufferAttachmentIndex{1}).ids[0],
                    .size = std::min(sizeof(BrickInstance) * (gpu_context.render_resolution.x * gpu_context.render_resolution.y + 1), sizeof(BrickInstance) * (MAX_BRICK_INSTANCE_COUNT + 1)),
                });
            },
            .name = "copy state",
        });

        return {color, depth, motion_vectors};
    }
} // namespace renderer

#endif
