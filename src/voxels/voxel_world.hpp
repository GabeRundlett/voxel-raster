#pragma once

struct VoxelBrickBitmask;
struct VoxelRenderAttribBrick;

struct VoxelWorld;

namespace voxel_world {
    auto create() -> VoxelWorld *;
    void destroy(VoxelWorld *self);

    void update(VoxelWorld *self);
    void load_model(VoxelWorld *self, char const *path);

    struct RayCastHit {
        int voxel_x, voxel_y, voxel_z;
        int nrm_x, nrm_y, nrm_z;
        float distance;
    };
    struct RayCastConfig {
        float const *ray_o;
        float const *ray_d;
        int max_iter = 512;
        float max_distance = 100.0f;
    };
    auto ray_cast(VoxelWorld *self, RayCastConfig const &config) -> RayCastHit;
    auto is_solid(VoxelWorld *self, float const *pos) -> bool;

    void apply_brush_a(VoxelWorld *self, int const *pos);
    void apply_brush_b(VoxelWorld *self, int const *pos);
} // namespace voxel_world

extern VoxelWorld *g_voxel_world;
