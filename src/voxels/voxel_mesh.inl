#pragma once

#include <daxa/utils/task_graph.inl>
#include <voxels/defs.inl>

struct Voxel {
    daxa_f32vec3 col;
    daxa_f32vec3 nrm;
};
struct PackedVoxel {
    daxa_u32 data;
};
struct VoxelBrickBitmask {
    daxa_u32 metadata;
    daxa_u32 neighbor_bits[(VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * 6 + 31) / 32];
    daxa_u32 bits[VOXELS_PER_BRICK / 32];
};
DAXA_DECL_BUFFER_PTR(VoxelBrickBitmask)
struct VoxelRenderAttribBrick {
    PackedVoxel packed_voxels[VOXELS_PER_BRICK];
};
DAXA_DECL_BUFFER_PTR(VoxelRenderAttribBrick)
struct VoxelBrickMesh {
    daxa_u32 face_count;
    daxa_u32 meshlet_start;
};
DAXA_DECL_BUFFER_PTR(VoxelBrickMesh)

struct PackedVoxelBrickFace {
    daxa_u32 data;
};
struct VoxelBrickFace {
    daxa_u32vec3 pos;
    daxa_u32 axis;
};

struct VoxelMeshlet {
    PackedVoxelBrickFace faces[MAX_FACES_PER_MESHLET];
};
DAXA_DECL_BUFFER_PTR(VoxelMeshlet)

struct VoxelMeshletMetadata {
    daxa_u32 brick_instance_index;
};
DAXA_DECL_BUFFER_PTR(VoxelMeshletMetadata)

struct VoxelMeshletAllocatorState {
    daxa_u32 hw_meshlet_count;
    daxa_u32 sw_meshlet_count;
    daxa_u32 _pad[30];
};
DAXA_DECL_BUFFER_PTR(VoxelMeshletAllocatorState)

struct BrickInstance {
    daxa_u32 chunk_index;
    daxa_u32 brick_index;
};
DAXA_DECL_BUFFER_PTR(BrickInstance)

struct BrickInstanceAllocatorState {
    daxa_u32 instance_count;
    daxa_u32 _pad[1];
};
DAXA_DECL_BUFFER_PTR(BrickInstanceAllocatorState)

struct Aabb {
    daxa_f32vec3 minimum;
    daxa_f32vec3 maximum;
};
DAXA_DECL_BUFFER_PTR(Aabb)

struct VoxelChunk {
    daxa_BufferPtr(VoxelBrickBitmask) bitmasks;
    daxa_BufferPtr(VoxelBrickMesh) meshes;
    daxa_BufferPtr(daxa_i32vec4) pos_scl;
    daxa_BufferPtr(Aabb) aabbs;
    daxa_BufferPtr(VoxelRenderAttribBrick) attribs;
    daxa_BufferPtr(daxa_u32) flags;
    daxa_u32 brick_n;
    daxa_f32vec3 pos;
};
DAXA_DECL_BUFFER_PTR(VoxelChunk)
