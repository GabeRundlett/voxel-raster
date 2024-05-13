#pragma once

struct VoxelBrickBitmask;

namespace voxel_world {
    struct State;
    using VoxelWorld = State *;

    void init(VoxelWorld &self);
    void deinit(VoxelWorld self);

    VoxelBrickBitmask *get_voxel_brick_bitmasks(VoxelWorld self);
    int *get_voxel_brick_positions(VoxelWorld self);
    unsigned int get_voxel_brick_count(VoxelWorld self);
    bool bricks_changed(VoxelWorld self);
    void update(VoxelWorld self);
} // namespace voxel_world
