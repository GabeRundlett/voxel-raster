#pragma once

struct Camera;
struct GpuInput;
struct Player;

namespace player {
    auto create() -> Player *;
    void destroy(Player *self);
    void on_key(Player *self, int key, int action, int mods);

    void on_mouse_move(Player *self, float x, float y);
    void on_mouse_scroll(Player *self, float x, float y);
    void on_mouse_button(Player *self, int button_id, int action);
    void on_key(Player *self, int key_id, int action);
    void on_resize(Player *self, int size_x, int size_y);

    void update(Player *self, float dt);
    void get_camera(Player *self, Camera *camera, GpuInput const *gpu_input);
    void get_observer_camera(Player *self, Camera *camera, GpuInput const *gpu_input);
    auto should_draw_from_observer(Player *self) -> bool;
} // namespace player
