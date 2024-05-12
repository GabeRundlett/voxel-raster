#pragma once

#include <daxa/utils/task_graph.inl>

#define INVALID_MESHLET_INDEX 0
// MUST never be greater than 124
#define MAX_TRIANGLES_PER_MESHLET 64
// MUST never be greater than MAX_TRIANGLES_PER_MESHLET * 3
#define MAX_VERTICES_PER_MESHLET 64

// MUST never be less than 2
#define VOXEL_BRICK_SIZE_LOG2 3
#define VOXEL_BRICK_SIZE (1u << VOXEL_BRICK_SIZE_LOG2)
#define VOXELS_PER_BRICK (VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE)

struct VoxelBrickBitmask {
    daxa_u32 bits[VOXELS_PER_BRICK / 32];
};
struct VoxelMeshlet {
    daxa_u32 voxel_brick_offset;
    daxa_u32 face_offset;
    daxa_u32 face_count;
};
struct VoxelBrickMesh {
    daxa_u32 meshlet_count;
    daxa_u32 meshlet_offset;
    daxa_i32vec4 pos_scl;
};
