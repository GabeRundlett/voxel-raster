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

struct DrawInput {
    daxa::TaskImageView task_visbuffer64;
    daxa::TaskImageView task_debug_overdraw;
    daxa::TaskImageView hiz;
    daxa::TaskBufferView task_indirect_infos;
    daxa::TaskBufferView task_chunks;
    daxa::TaskBufferView task_brick_data;
    daxa::TaskBufferView task_brick_instance_allocator;
    daxa::TaskBufferView task_brick_meshlet_allocator;
    daxa::TaskBufferView task_brick_meshlet_metadata;
    uint32_t indirect_offset;
    bool draw_from_observer = false;
};

void draw_visbuffer(GpuContext &gpu_context, daxa::TaskGraph &task_graph, DrawInput const &input) {
    auto extra_defines = std::vector<daxa::ShaderDefine>{};
    if (input.hiz != daxa::NullTaskImage) {
        extra_defines.push_back({"DO_DEPTH_CULL", "1"});
    }
    if (input.draw_from_observer) {
        extra_defines.push_back({"DRAW_FROM_OBSERVER", "1"});
    }

    gpu_context.add(RasterTask<DrawVisbuffer::Task, DrawVisbufferPush, uint32_t>{
        .mesh_source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"},
        .frag_source = daxa::ShaderFile{"voxel_rasterizer/draw_visbuffer.glsl"},
        .raster = {.primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST},
        .required_subgroup_size = 32,
        .extra_defines = extra_defines,
        .views = std::array{
            daxa::attachment_view(DrawVisbuffer::AT.visbuffer64, input.task_visbuffer64),
            daxa::attachment_view(DrawVisbuffer::AT.gpu_input, gpu_context.task_input_buffer),
            daxa::attachment_view(DrawVisbuffer::AT.chunks, input.task_chunks),
            daxa::attachment_view(DrawVisbuffer::AT.brick_data, input.task_brick_data),
            daxa::attachment_view(DrawVisbuffer::AT.brick_instance_allocator, input.task_brick_instance_allocator),
            daxa::attachment_view(DrawVisbuffer::AT.meshlet_allocator, input.task_brick_meshlet_allocator),
            daxa::attachment_view(DrawVisbuffer::AT.meshlet_metadata, input.task_brick_meshlet_metadata),
#if ENABLE_DEBUG_VIS
            daxa::attachment_view(DrawVisbuffer::AT.debug_overdraw, input.task_debug_overdraw),
#endif
            daxa::attachment_view(DrawVisbuffer::AT.indirect_info, input.task_indirect_infos),
            daxa::attachment_view(DrawVisbuffer::AT.hiz, input.hiz),
        },
        .callback_ = [](daxa::TaskInterface const &ti, daxa::RasterPipeline &pipeline, DrawVisbufferPush &push, uint32_t const &indirect_offset) {
            auto const &image_attach_info = ti.get(DrawVisbuffer::AT.visbuffer64);
            auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
            auto render_recorder = std::move(ti.recorder).begin_renderpass({.render_area = {.width = image_info.size.x, .height = image_info.size.y}});
            render_recorder.set_pipeline(pipeline);
            set_push_constant(ti, render_recorder, push);
            render_recorder.draw_mesh_tasks_indirect({.indirect_buffer = ti.get(DrawVisbuffer::AT.indirect_info).ids[0], .offset = indirect_offset, .draw_count = 1, .stride = {}});
            ti.recorder = std::move(render_recorder).end_renderpass();
        },
        .info = input.indirect_offset,
        .task_graph = &task_graph,
    });
}

void compute_rasterize(GpuContext &gpu_context, daxa::TaskGraph &task_graph, DrawInput const &input) {
    auto extra_defines = std::vector<daxa::ShaderDefine>{};
    if (input.hiz != daxa::NullTaskImage) {
        extra_defines.push_back({"DO_DEPTH_CULL", "1"});
    }
    if (input.draw_from_observer) {
        extra_defines.push_back({"DRAW_FROM_OBSERVER", "1"});
    }

    gpu_context.add(ComputeTask<ComputeRasterize::Task, ComputeRasterizePush, uint32_t>{
        .source = daxa::ShaderFile{"voxel_rasterizer/compute_rasterize.glsl"},
        .required_subgroup_size = 32,
        .extra_defines = extra_defines,
        .views = std::array{
            daxa::attachment_view(ComputeRasterize::AT.visbuffer64, input.task_visbuffer64),
            daxa::attachment_view(ComputeRasterize::AT.gpu_input, gpu_context.task_input_buffer),
            daxa::attachment_view(ComputeRasterize::AT.chunks, input.task_chunks),
            daxa::attachment_view(ComputeRasterize::AT.brick_data, input.task_brick_data),
            daxa::attachment_view(ComputeRasterize::AT.brick_instance_allocator, input.task_brick_instance_allocator),
            daxa::attachment_view(ComputeRasterize::AT.meshlet_allocator, input.task_brick_meshlet_allocator),
            daxa::attachment_view(ComputeRasterize::AT.meshlet_metadata, input.task_brick_meshlet_metadata),
            daxa::attachment_view(ComputeRasterize::AT.indirect_info, input.task_indirect_infos),
#if ENABLE_DEBUG_VIS
            daxa::attachment_view(ComputeRasterize::AT.debug_overdraw, input.task_debug_overdraw),
#endif
            daxa::attachment_view(ComputeRasterize::AT.hiz, input.hiz),

        },
        .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, ComputeRasterizePush &push, uint32_t const &indirect_offset) {
            ti.recorder.set_pipeline(pipeline);
            set_push_constant(ti, push);
            ti.recorder.dispatch_indirect({.indirect_buffer = ti.get(ComputeRasterize::AT.indirect_info).ids[0], .offset = indirect_offset});
        },
        .info = input.indirect_offset,
        .task_graph = &task_graph,
    });
}

auto task_gen_hiz_single_pass(GpuContext &gpu_context, daxa::TaskGraph &task_graph, daxa::TaskImageView task_visbuffer64) -> daxa::TaskImageView {
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

    gpu_context.add(ComputeTask<GenHiz::Task, GenHizPush, NoTaskInfo>{
        .source = daxa::ShaderFile{"voxel_rasterizer/gen_hiz.glsl"},
        .views = std::array{
            daxa::attachment_view(GenHiz::AT.gpu_input, gpu_context.task_input_buffer),
            daxa::attachment_view(GenHiz::AT.src, task_visbuffer64),
            daxa::attachment_view(GenHiz::AT.mips, task_hiz),
        },
        .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, GenHizPush &push, NoTaskInfo const &) {
            ti.recorder.set_pipeline(pipeline);
            auto const &image_attach_info = ti.get(GenHiz::AT.src);
            auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
            auto const dispatch_x = round_up_div(find_next_lower_po2(image_info.size.x) * 2, GEN_HIZ_WINDOW_X);
            auto const dispatch_y = round_up_div(find_next_lower_po2(image_info.size.y) * 2, GEN_HIZ_WINDOW_Y);
            push = {
                .counter = ti.allocator->allocate_fill(0u).value().device_address,
                .mip_count = ti.get(GenHiz::AT.mips).view.slice.level_count,
                .total_workgroup_count = dispatch_x * dispatch_y,
            };
            set_push_constant(ti, push);
            ti.recorder.dispatch({dispatch_x, dispatch_y, 1});
        },
        .task_graph = &task_graph,
    });

    return task_hiz;
}

namespace renderer {
    struct VoxelRasterizer {
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

    void init(VoxelRasterizer *self, daxa::Device &device) {
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

    void update(VoxelRasterizer *self, GpuContext &gpu_context, daxa::TaskBuffer &task_chunks, daxa::TaskBuffer &task_brick_data) {
        auto temp_task_graph = daxa::TaskGraph({
            .device = gpu_context.device,
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

        gpu_context.add(ComputeTask<SetIndirectInfos::Task, SetIndirectInfosPush, NoTaskInfo>{
            .source = daxa::ShaderFile{"voxel_rasterizer/set_indirect_infos.glsl"},
            .extra_defines = {{"SET_TYPE", "0"}},
            .views = std::array{
                daxa::attachment_view(SetIndirectInfos::AT.gpu_input, task_input_buffer),
                daxa::attachment_view(SetIndirectInfos::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(SetIndirectInfos::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(SetIndirectInfos::AT.indirect_info, task_indirect_infos),
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, SetIndirectInfosPush &push, NoTaskInfo const &) {
                ti.recorder.set_pipeline(pipeline);
                set_push_constant(ti, push);
                ti.recorder.dispatch({1, 1, 1});
            },
            .task_graph = &temp_task_graph,
        });

        gpu_context.add(ComputeTask<ClearDrawFlags::Task, ClearDrawFlagsPush, NoTaskInfo>{
            .source = daxa::ShaderFile{"voxel_rasterizer/clear_draw_flags.glsl"},
            .views = std::array{
                daxa::attachment_view(ClearDrawFlags::AT.chunks, task_chunks),
                daxa::attachment_view(ClearDrawFlags::AT.brick_data, task_brick_data),
                daxa::attachment_view(ClearDrawFlags::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(ClearDrawFlags::AT.indirect_info, task_indirect_infos),
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, ClearDrawFlagsPush &push, NoTaskInfo const &) {
                ti.recorder.set_pipeline(pipeline);
                set_push_constant(ti, push);
                ti.recorder.dispatch_indirect({.indirect_buffer = ti.get(ClearDrawFlags::AT.indirect_info).ids[0]});
            },
            .task_graph = &temp_task_graph,
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

            gpu_context.add(ComputeTask<SetIndirectInfos::Task, SetIndirectInfosPush, NoTaskInfo>{
                .source = daxa::ShaderFile{"voxel_rasterizer/set_indirect_infos.glsl"},
                .extra_defines = {{"SET_TYPE", first_draw ? "0" : "1"}},
                .views = std::array{
                    daxa::attachment_view(SetIndirectInfos::AT.gpu_input, gpu_context.task_input_buffer),
                    daxa::attachment_view(SetIndirectInfos::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(SetIndirectInfos::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(SetIndirectInfos::AT.indirect_info, task_indirect_infos),
                },
                .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, SetIndirectInfosPush &push, NoTaskInfo const &) {
                    ti.recorder.set_pipeline(pipeline);
                    set_push_constant(ti, push);
                    ti.recorder.dispatch({1, 1, 1});
                },
                .task_graph = &task_graph,
            });

            gpu_context.add(ComputeTask<MeshVoxelBricks::Task, MeshVoxelBricksPush, size_t>{
                .source = daxa::ShaderFile{"voxel_rasterizer/mesh_voxel_bricks.glsl"},
                .views = std::array{
                    daxa::attachment_view(MeshVoxelBricks::AT.gpu_input, gpu_context.task_input_buffer),
                    daxa::attachment_view(MeshVoxelBricks::AT.chunks, task_chunks),
                    daxa::attachment_view(MeshVoxelBricks::AT.brick_data, task_brick_data),
                    daxa::attachment_view(MeshVoxelBricks::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(MeshVoxelBricks::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(MeshVoxelBricks::AT.meshlet_metadata, self->task_brick_meshlet_metadata),
                    daxa::attachment_view(MeshVoxelBricks::AT.indirect_info, task_indirect_infos),
                },
                .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, MeshVoxelBricksPush &push, size_t const &indirect_offset) {
                    ti.recorder.set_pipeline(pipeline);
                    set_push_constant(ti, push);
                    ti.recorder.dispatch_indirect({.indirect_buffer = ti.get(MeshVoxelBricks::AT.indirect_info).ids[0], .offset = indirect_offset});
                },
                .info = sizeof(DispatchIndirectStruct) * 0,
                .task_graph = &task_graph,
            });

            gpu_context.add(ComputeTask<SetIndirectInfos::Task, SetIndirectInfosPush, NoTaskInfo>{
                .source = daxa::ShaderFile{"voxel_rasterizer/set_indirect_infos.glsl"},
                .extra_defines = {{"SET_TYPE", "2"}},
                .views = std::array{
                    daxa::attachment_view(SetIndirectInfos::AT.gpu_input, gpu_context.task_input_buffer),
                    daxa::attachment_view(SetIndirectInfos::AT.brick_instance_allocator, task_brick_instance_allocator),
                    daxa::attachment_view(SetIndirectInfos::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                    daxa::attachment_view(SetIndirectInfos::AT.indirect_info, task_indirect_infos),
                },
                .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, SetIndirectInfosPush &push, NoTaskInfo const &) {
                    ti.recorder.set_pipeline(pipeline);
                    set_push_constant(ti, push);
                    ti.recorder.dispatch({1, 1, 1});
                },
                .task_graph = &task_graph,
            });

            draw_visbuffer(
                gpu_context, task_graph,
                {
                    .task_visbuffer64 = task_visbuffer64,
#if ENABLE_DEBUG_VIS
                    .task_debug_overdraw = task_debug_overdraw,
#endif
                    .hiz = hiz,
                    .task_indirect_infos = task_indirect_infos,
                    .task_chunks = task_chunks,
                    .task_brick_data = task_brick_data,
                    .task_brick_instance_allocator = task_brick_instance_allocator,
                    .task_brick_meshlet_allocator = self->task_brick_meshlet_allocator,
                    .task_brick_meshlet_metadata = self->task_brick_meshlet_metadata,
                    .indirect_offset = sizeof(DispatchIndirectStruct) * 3,
                    .draw_from_observer = false,
                });

            compute_rasterize(
                gpu_context, task_graph,
                {
                    .task_visbuffer64 = task_visbuffer64,
#if ENABLE_DEBUG_VIS
                    .task_debug_overdraw = task_debug_overdraw,
#endif
                    .hiz = hiz,
                    .task_indirect_infos = task_indirect_infos,
                    .task_chunks = task_chunks,
                    .task_brick_data = task_brick_data,
                    .task_brick_instance_allocator = task_brick_instance_allocator,
                    .task_brick_meshlet_allocator = self->task_brick_meshlet_allocator,
                    .task_brick_meshlet_metadata = self->task_brick_meshlet_metadata,
                    .indirect_offset = sizeof(DispatchIndirectStruct) * 1,
                    .draw_from_observer = false,
                });

            if (draw_from_observer) {
                draw_visbuffer(
                    gpu_context, task_graph,
                    {
                        .task_visbuffer64 = task_observer_visbuffer64,
#if ENABLE_DEBUG_VIS
                        .task_debug_overdraw = task_debug_overdraw,
#endif
                        .hiz = hiz,
                        .task_indirect_infos = task_indirect_infos,
                        .task_chunks = task_chunks,
                        .task_brick_data = task_brick_data,
                        .task_brick_instance_allocator = task_brick_instance_allocator,
                        .task_brick_meshlet_allocator = self->task_brick_meshlet_allocator,
                        .task_brick_meshlet_metadata = self->task_brick_meshlet_metadata,
                        .indirect_offset = sizeof(DispatchIndirectStruct) * 3,
                        .draw_from_observer = true,
                    });

                compute_rasterize(
                    gpu_context, task_graph,
                    {
                        .task_visbuffer64 = task_observer_visbuffer64,
#if ENABLE_DEBUG_VIS
                        .task_debug_overdraw = task_debug_overdraw,
#endif
                        .hiz = hiz,
                        .task_indirect_infos = task_indirect_infos,
                        .task_chunks = task_chunks,
                        .task_brick_data = task_brick_data,
                        .task_brick_instance_allocator = task_brick_instance_allocator,
                        .task_brick_meshlet_allocator = self->task_brick_meshlet_allocator,
                        .task_brick_meshlet_metadata = self->task_brick_meshlet_metadata,
                        .indirect_offset = sizeof(DispatchIndirectStruct) * 1,
                        .draw_from_observer = true,
                    });
            }

            first_draw = false;
        };

        // draw previously visible bricks
        draw_brick_instances(self->task_brick_instance_allocator, daxa::NullTaskImage);

        // build hi-z
        auto hiz = task_gen_hiz_single_pass(gpu_context, task_graph, task_visbuffer64);

        // cull and draw the rest
        gpu_context.add(ComputeTask<AllocateBrickInstances::Task, AllocateBrickInstancesPush, uint32_t const *>{
            .source = daxa::ShaderFile{"voxel_rasterizer/allocate_brick_instances.glsl"},
            .views = std::array{
                daxa::attachment_view(AllocateBrickInstances::AT.gpu_input, gpu_context.task_input_buffer),
                daxa::attachment_view(AllocateBrickInstances::AT.chunks, task_chunks),
                daxa::attachment_view(AllocateBrickInstances::AT.brick_data, task_brick_data),
                daxa::attachment_view(AllocateBrickInstances::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(AllocateBrickInstances::AT.hiz, hiz),
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, AllocateBrickInstancesPush &push, uint32_t const *const &chunk_n) {
                ti.recorder.set_pipeline(pipeline);
                set_push_constant(ti, push);
                ti.recorder.dispatch({*chunk_n, 1, 1});
            },
            .info = &chunk_n,
            .task_graph = &task_graph,
        });

        draw_brick_instances(self->task_brick_instance_allocator, hiz);

        // clear draw flags. This needs to be done before the Analyze visbuffer pass,
        // since AnalyzeVisbuffer populates them again.
        gpu_context.add(ComputeTask<SetIndirectInfos::Task, SetIndirectInfosPush, NoTaskInfo>{
            .source = daxa::ShaderFile{"voxel_rasterizer/set_indirect_infos.glsl"},
            .extra_defines = {{"SET_TYPE", "0"}},
            .views = std::array{
                daxa::attachment_view(SetIndirectInfos::AT.gpu_input, gpu_context.task_input_buffer),
                daxa::attachment_view(SetIndirectInfos::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(SetIndirectInfos::AT.meshlet_allocator, self->task_brick_meshlet_allocator),
                daxa::attachment_view(SetIndirectInfos::AT.indirect_info, task_indirect_infos),
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, SetIndirectInfosPush &push, NoTaskInfo const &) {
                ti.recorder.set_pipeline(pipeline);
                set_push_constant(ti, push);
                ti.recorder.dispatch({1, 1, 1});
            },
            .task_graph = &task_graph,
        });

        gpu_context.add(ComputeTask<ClearDrawFlags::Task, ClearDrawFlagsPush, NoTaskInfo>{
            .source = daxa::ShaderFile{"voxel_rasterizer/clear_draw_flags.glsl"},
            .views = std::array{
                daxa::attachment_view(ClearDrawFlags::AT.chunks, task_chunks),
                daxa::attachment_view(ClearDrawFlags::AT.brick_data, task_brick_data),
                daxa::attachment_view(ClearDrawFlags::AT.brick_instance_allocator, self->task_brick_instance_allocator),
                daxa::attachment_view(ClearDrawFlags::AT.indirect_info, task_indirect_infos),
            },
            .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, ClearDrawFlagsPush &push, NoTaskInfo const &) {
                ti.recorder.set_pipeline(pipeline);
                set_push_constant(ti, push);
                ti.recorder.dispatch_indirect({.indirect_buffer = ti.get(ClearDrawFlags::AT.indirect_info).ids[0]});
            },
            .task_graph = &task_graph,
        });

        task_fill_buffer(task_graph, self->task_visible_brick_instance_allocator, uint32_t{0});
        clear_task_buffers(task_graph, std::array<daxa::TaskBufferView, 1>{self->task_brick_visibility_bits}, std::array{daxa::BufferClearInfo{.size = sizeof(uint32_t) * (MAX_BRICK_INSTANCE_COUNT + 1)}});
        gpu_context.add(ComputeTask<AnalyzeVisbuffer::Task, AnalyzeVisbufferPush, NoTaskInfo>{
            .source = daxa::ShaderFile{"voxel_rasterizer/analyze_visbuffer.glsl"},
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
            .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, AnalyzeVisbufferPush &push, NoTaskInfo const &) {
                auto const &image_attach_info = ti.get(AnalyzeVisbuffer::AT.visbuffer64);
                auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
                ti.recorder.set_pipeline(pipeline);
                set_push_constant(ti, push);
                ti.recorder.dispatch({round_up_div(image_info.size.x, 16), round_up_div(image_info.size.y, 16), 1});
            },
            .task_graph = &task_graph,
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

        gpu_context.add(ComputeTask<ShadeVisbuffer::Task, ShadeVisbufferPush, NoTaskInfo>{
            .source = daxa::ShaderFile{"voxel_rasterizer/shade_visbuffer.glsl"},
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
            .callback_ = [](daxa::TaskInterface const &ti, daxa::ComputePipeline &pipeline, ShadeVisbufferPush &push, NoTaskInfo const &) {
                auto const &image_attach_info = ti.get(ShadeVisbuffer::AT.color);
                auto image_info = ti.device.info_image(image_attach_info.ids[0]).value();
                ti.recorder.set_pipeline(pipeline);
                set_push_constant(ti, push);
                ti.recorder.dispatch({round_up_div(image_info.size.x, 16), round_up_div(image_info.size.y, 16), 1});
            },
            .task_graph = &task_graph,
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
