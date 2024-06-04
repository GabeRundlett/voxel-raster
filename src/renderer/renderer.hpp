#pragma once

namespace player {
    struct State;
    using Player = State *;
} // namespace player

namespace voxel_world {
    struct State;
    using VoxelWorld = State *;
}; // namespace voxel_world

struct VoxelBrickBitmask;
struct VoxelAttribBrick;

namespace renderer {
    struct State;
    using Renderer = State *;

    struct ChunkState;
    using Chunk = ChunkState *;

    void init(Renderer &self, void *glfw_window_ptr);
    void deinit(Renderer self);

    void on_resize(Renderer self, int size_x, int size_y);
    void draw(Renderer self, player::Player player, voxel_world::VoxelWorld voxel_world);
    void toggle_wireframe(Renderer self);
    void submit_debug_lines(float const *lines, int line_n);

    void init(Chunk &self);
    void deinit(Chunk self);
    void update(Chunk self, int brick_count, VoxelBrickBitmask const *bitmasks, VoxelAttribBrick const *attribs, int const *positions);
    void render_chunk(Chunk self, float const *pos);
} // namespace renderer
