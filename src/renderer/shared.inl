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
    daxa_u32 brick_n;
    daxa_f32 time;
    Camera cam;
};
DAXA_DECL_BUFFER_PTR(GpuInput)

struct BrickInstance {
    daxa_u32 chunk_index;
    daxa_u32 brick_index;
};
DAXA_DECL_BUFFER_PTR(BrickInstance)

DAXA_DECL_TASK_HEAD_BEGIN(MeshVoxelBricksH)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VoxelBrickBitmask), bitmasks)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(VoxelBrickMesh), meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(VoxelMeshletMetadata), meshlet_metadata)
DAXA_DECL_TASK_HEAD_END

struct MeshVoxelBricksPush {
    DAXA_TH_BLOB(MeshVoxelBricksH, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(DrawVisbufferH)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_BUFFER_PTR(TASK_SHADER_READ, daxa_BufferPtr(VoxelBrickMesh), meshes)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(daxa_i32vec4), pos_scl)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(VoxelMeshlet), meshlet_allocator)
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
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(VoxelBrickMesh), meshes)
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
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(VoxelBrickMesh), meshes)
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(VoxelMeshlet), meshlet_allocator)
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(VoxelMeshletMetadata), meshlet_metadata)
DAXA_TH_IMAGE_INDEX(COMPUTE_SHADER_SAMPLED, REGULAR_2D, visbuffer)
DAXA_DECL_TASK_HEAD_END

struct AnalyzeVisbufferPush {
    DAXA_TH_BLOB(AnalyzeVisbufferH, uses)
};
