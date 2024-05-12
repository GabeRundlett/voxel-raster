#pragma once

namespace player {
    struct State;
    using Player = State *;
} // namespace player

namespace voxel_world {
    struct State;
    using VoxelWorld = State *;
}; // namespace voxel_world

namespace renderer {
    struct State;
    using Renderer = State *;

    void init(Renderer &self, void *glfw_window_ptr);
    void deinit(Renderer self);

    void on_resize(Renderer self, int size_x, int size_y);
    void draw(Renderer self, player::Player player, voxel_world::VoxelWorld voxel_world);
} // namespace renderer
