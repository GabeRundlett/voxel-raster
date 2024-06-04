#pragma once

struct VoxelBrickBitmask;
struct VoxelAttribBrick;

namespace voxel_world {
    struct State;
    using VoxelWorld = State *;

    void init(VoxelWorld &self);
    void deinit(VoxelWorld self);

    void update(VoxelWorld self);
    void load_model(char const *path);
} // namespace voxel_world
