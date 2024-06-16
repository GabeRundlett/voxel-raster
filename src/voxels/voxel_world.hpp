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

    struct RayCastHit {
        int voxel_x, voxel_y, voxel_z;
        float distance;
    };
    auto ray_cast(float const *ray_o, float const *ray_d) -> RayCastHit;
} // namespace voxel_world
