#pragma once

struct VoxelBrickBitmask;
struct VoxelAttribBrick;

namespace voxel_world {
    struct State;
    using VoxelWorld = State *;

    void init(VoxelWorld &self);
    void deinit(VoxelWorld self);

    void update(VoxelWorld self);
    void load_model(VoxelWorld self, char const *path);

    struct RayCastHit {
        int voxel_x, voxel_y, voxel_z;
        int nrm_x, nrm_y, nrm_z;
        float distance;
    };
    auto ray_cast(VoxelWorld self, float const *ray_o, float const *ray_d) -> RayCastHit;
    auto is_solid(VoxelWorld self, float const *pos) -> bool;

    void apply_brush_a(VoxelWorld self, int const *pos);
    void apply_brush_b(VoxelWorld self, int const *pos);
} // namespace voxel_world

extern voxel_world::VoxelWorld g_voxel_world;
