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
        int nrm_x, nrm_y, nrm_z;
        float distance;
    };
    auto ray_cast(float const *ray_o, float const *ray_d) -> RayCastHit;
    auto is_solid(float const *pos) -> bool;

    void apply_brush_a(int const *pos);
    void apply_brush_b(int const *pos);
} // namespace voxel_world
