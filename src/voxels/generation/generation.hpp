#pragma once

using RandomCtx = const unsigned char *;

struct MinMax {
    float min;
    float max;
};

MinMax voxel_minmax_value_cpp(RandomCtx random_ctx, float p0x, float p0y, float p0z, float p1x, float p1y, float p1z);

#if USE_ISPC
#include <generation_ispc.h>
#else

void generate_bitmask_cpp(
    int brick_xi, int brick_yi, int brick_zi,
    int chunk_xi, int chunk_yi, int chunk_zi,
    int level_i, unsigned int bits[], unsigned int *metadata,
    RandomCtx random_ctx);

void generate_attributes_cpp(
    int brick_xi, int brick_yi, int brick_zi,
    int chunk_xi, int chunk_yi, int chunk_zi,
    int level_i, unsigned int packed_voxels[],
    RandomCtx random_ctx);

#endif

static inline void generate_bitmask(
    int brick_xi, int brick_yi, int brick_zi,
    int chunk_xi, int chunk_yi, int chunk_zi,
    int level_i, unsigned int bits[], unsigned int *metadata,
    RandomCtx random_ctx) {
#if USE_ISPC
    ispc::generate_bitmask(brick_xi, brick_yi, brick_zi, chunk_xi, chunk_yi, chunk_zi, level_i, bits, metadata, random_ctx);
#else
    generate_bitmask_cpp(brick_xi, brick_yi, brick_zi, chunk_xi, chunk_yi, chunk_zi, level_i, bits, metadata, random_ctx);
#endif
}

void generate_attributes(
    int brick_xi, int brick_yi, int brick_zi,
    int chunk_xi, int chunk_yi, int chunk_zi,
    int level_i, unsigned int packed_voxels[],
    RandomCtx random_ctx) {
#if USE_ISPC
    ispc::generate_attributes(brick_xi, brick_yi, brick_zi, chunk_xi, chunk_yi, chunk_zi, level_i, packed_voxels, random_ctx);
#else
    generate_attributes_cpp(brick_xi, brick_yi, brick_zi, chunk_xi, chunk_yi, chunk_zi, level_i, packed_voxels, random_ctx);
#endif
}
