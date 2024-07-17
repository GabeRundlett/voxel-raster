#pragma once

struct Player;
struct VoxelWorld;

struct VoxelBrickBitmask;
struct VoxelRenderAttribBrick;
struct Renderer;

namespace renderer {
    struct Chunk;

    auto create(void *glfw_window_ptr) -> Renderer *;
    void destroy(Renderer *self);

    void on_resize(Renderer *self, int size_x, int size_y);
    void draw(Renderer *self, Player *player, VoxelWorld *voxel_world);
    void toggle_vsync(Renderer *self);
    void toggle_fsr2(Renderer *self);
    void toggle_rt(Renderer *self);
    void toggle_shadows(Renderer *self);

    struct Line {
        float p0_x, p0_y, p0_z;
        float p1_x, p1_y, p1_z;
        float r, g, b;
    };
    struct Point {
        float p0_x, p0_y, p0_z;
        float r, g, b;
        float s_x, s_y, type;
    };
    struct Box {
        float p0_x, p0_y, p0_z;
        float p1_x, p1_y, p1_z;
        float r, g, b;
    };

    void submit_debug_lines(Renderer *self, Line const *lines, int line_n);
    void submit_debug_points(Renderer *self, Point const *points, int point_n);
    void submit_debug_box_lines(Renderer *self, Box const *cubes, int cube_n);

    auto create_chunk(Renderer *self) -> Chunk *;
    void destroy_chunk(Renderer *self, Chunk *chunk);
    void update(Chunk *self, int brick_count, int const *surface_brick_indices, VoxelBrickBitmask const *bitmasks, VoxelRenderAttribBrick const *const *attribs, int const *positions);
    void render_chunk(Renderer *self, Chunk *chunk, float const *pos);
} // namespace renderer

extern Renderer *g_renderer;
