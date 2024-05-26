#pragma once

struct VoxelBrickBitmask;

namespace voxel_world {
    struct State;
    using VoxelWorld = State *;

    void init(VoxelWorld &self);
    void deinit(VoxelWorld self);

    VoxelBrickBitmask *get_voxel_brick_bitmasks(VoxelWorld self, unsigned int chunk_index);
    int *get_voxel_brick_positions(VoxelWorld self, unsigned int chunk_index);
    unsigned int get_voxel_brick_count(VoxelWorld self, unsigned int chunk_index);
    bool chunk_bricks_changed(VoxelWorld self, unsigned int chunk_index);
    unsigned int get_chunk_count(VoxelWorld self);
    void get_chunk_pos(VoxelWorld self, unsigned int chunk_index, float *o_pos);
    void update(VoxelWorld self);
    void load_model(char const *path);
} // namespace voxel_world
