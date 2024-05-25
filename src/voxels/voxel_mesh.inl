#pragma once

#include <daxa/utils/task_graph.inl>

#define INVALID_MESHLET_INDEX 0
#define MAX_FACES_PER_MESHLET 32
// MUST never be greater than 124
#define MAX_TRIANGLES_PER_MESHLET (MAX_FACES_PER_MESHLET * 2)
// MUST never be greater than MAX_TRIANGLES_PER_MESHLET * 3
#define MAX_VERTICES_PER_MESHLET (MAX_FACES_PER_MESHLET * 4)

// MUST never be less than 2
#define VOXEL_BRICK_SIZE_LOG2 3
#define VOXEL_BRICK_SIZE (1u << VOXEL_BRICK_SIZE_LOG2)
#define VOXELS_PER_BRICK (VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE)
#define MAX_INNER_FACES_PER_BRICK ((VOXEL_BRICK_SIZE - 1) * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * 3)
#define MAX_OUTER_FACES_PER_BRICK ((VOXEL_BRICK_SIZE + 1) * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * 3)
#define MAX_MESHLETS_PER_BRICK ((MAX_OUTER_FACES_PER_BRICK + MAX_TRIANGLES_PER_MESHLET - 1) / MAX_TRIANGLES_PER_MESHLET)

#define BRICK_CHUNK_SIZE_LOG2 4
#define BRICK_CHUNK_SIZE (1u << BRICK_CHUNK_SIZE_LOG2)
#define BRICKS_PER_CHUNK (BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE)

#define VOXEL_CHUNK_SIZE_LOG2 (VOXEL_BRICK_SIZE_LOG2 + BRICK_CHUNK_SIZE_LOG2)
#define VOXEL_CHUNK_SIZE (1u << VOXEL_CHUNK_SIZE_LOG2)

#define MAX_MESHLET_COUNT (1 << 24)
#define MAX_BRICK_COUNT (1 << 20)
#define MAX_CHUNK_COUNT (1 << 16)

#if defined(__cplusplus)
// Below, we pack meshlet start offsets into 8-bit integers. Since
// we will start only on the bounds of a uint, we divide the face
// count by 32 and ensure that number is representable in 8 bits
static_assert(MAX_OUTER_FACES_PER_BRICK / 32 < 256);
#endif

struct VoxelBrickBitmask {
    daxa_u32 bits[VOXELS_PER_BRICK / 32];
};
DAXA_DECL_BUFFER_PTR(VoxelBrickBitmask)
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
    PackedVoxelBrickFace faces[32];
};
DAXA_DECL_BUFFER_PTR(VoxelMeshlet)

struct VoxelMeshletMetadata {
    daxa_u32 brick_id;
};
DAXA_DECL_BUFFER_PTR(VoxelMeshletMetadata)

struct VoxelMeshletAllocatorState {
    daxa_u32 meshlet_count;
    daxa_u32 _pad[31];
};
DAXA_DECL_BUFFER_PTR(VoxelMeshletAllocatorState)
