#pragma once

#include <daxa/utils/task_graph.inl>
#include "camera.inl"
#include <voxels/voxel_mesh.inl>

#define ENABLE_DEBUG_VIS 1

struct DispatchIndirectStruct {
    daxa_u32 x;
    daxa_u32 y;
    daxa_u32 z;
};
DAXA_DECL_BUFFER_PTR(DispatchIndirectStruct)

struct GpuInput {
    daxa_u32vec2 render_size;
    daxa_u32 chunk_n;
    daxa_f32 time;
    Camera cam;
};
DAXA_DECL_BUFFER_PTR(GpuInput)

DAXA_DECL_TASK_HEAD_BEGIN(AllocateBrickInstancesH)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(BrickInstance), brick_instance_allocator)
DAXA_DECL_TASK_HEAD_END

struct AllocateBrickInstancesPush {
    DAXA_TH_BLOB(AllocateBrickInstancesH, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(SetIndirectInfos0H)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstanceAllocatorState), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(DispatchIndirectStruct), indirect_infos)
DAXA_DECL_TASK_HEAD_END

struct SetIndirectInfos0Push {
    DAXA_TH_BLOB(SetIndirectInfos0H, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(MeshVoxelBricksH)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ, bitmasks)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ_WRITE, meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(VoxelMeshletMetadata), meshlet_metadata)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ, indirect_info)
DAXA_DECL_TASK_HEAD_END

struct MeshVoxelBricksPush {
    DAXA_TH_BLOB(MeshVoxelBricksH, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(DrawVisbufferH)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(TASK_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(TASK_SHADER_READ, meshes)
DAXA_TH_BUFFER(MESH_SHADER_READ, pos_scl)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER(DRAW_INDIRECT_INFO_READ, indirect_info)
#if ENABLE_DEBUG_VIS
DAXA_TH_IMAGE_INDEX(FRAGMENT_SHADER_STORAGE_READ_WRITE, REGULAR_2D, debug_overdraw)
#endif
DAXA_DECL_TASK_HEAD_END

struct DrawVisbufferPush {
    DAXA_TH_BLOB(DrawVisbufferH, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(ShadeVisbufferH)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(FRAGMENT_SHADER_READ, meshes)
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(VoxelMeshletMetadata), meshlet_metadata)
#if ENABLE_DEBUG_VIS
DAXA_TH_IMAGE_INDEX(FRAGMENT_SHADER_SAMPLED, REGULAR_2D, debug_overdraw)
#endif
DAXA_TH_IMAGE_INDEX(FRAGMENT_SHADER_SAMPLED, REGULAR_2D, visbuffer)
DAXA_DECL_TASK_HEAD_END

struct ShadeVisbufferPush {
    DAXA_TH_BLOB(ShadeVisbufferH, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(AnalyzeVisbufferH)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelChunk), chunks)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ, meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(BrickInstance), brick_instance_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelMeshletMetadata), meshlet_metadata)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(daxa_u32), brick_visibility_bits)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(BrickInstance), visible_brick_instance_allocator)
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_SAMPLED, REGULAR_2D, visbuffer)
DAXA_DECL_TASK_HEAD_END

struct AnalyzeVisbufferPush {
    DAXA_TH_BLOB(AnalyzeVisbufferH, uses)
};
