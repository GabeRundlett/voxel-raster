#pragma once

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

#define BRICK_CHUNK_SIZE_LOG2 3
#define BRICK_CHUNK_SIZE (1u << BRICK_CHUNK_SIZE_LOG2)
#define BRICKS_PER_CHUNK (BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE)

#define VOXEL_CHUNK_SIZE_LOG2 (VOXEL_BRICK_SIZE_LOG2 + BRICK_CHUNK_SIZE_LOG2)
#define VOXEL_CHUNK_SIZE (1u << VOXEL_CHUNK_SIZE_LOG2)

#define MAX_MESHLET_COUNT (1 << 21)
#define MAX_SW_MESHLET_COUNT (MAX_MESHLET_COUNT * 3 / 4)
#define MAX_CHUNK_COUNT (1 << 16)
#define MAX_BRICK_INSTANCE_COUNT (1 << 20)

#define RANDOM_BUFFER_SIZE_LOG2 8
#define RANDOM_BUFFER_SIZE (1 << RANDOM_BUFFER_SIZE_LOG2)

#if defined(__cplusplus)
using RandomCtx = unsigned char const *;
#elif ISPC
#define RandomCtx const uint8 *uniform
#endif

struct MinMax {
    float min;
    float max;
};

struct NoiseSettings {
    float persistence;
    float lacunarity;
    float scale;
    float amplitude;
    int octaves;
};
