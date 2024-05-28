#pragma once

struct Camera;

namespace player {
    struct State;
    using Player = State *;

    void init(Player &self);
    void deinit(Player self);
    void on_key(Player self, int key, int action, int mods);

    void on_mouse_move(Player self, float x, float y);
    void on_mouse_scroll(Player self, float x, float y);
    void on_key(Player self, int key_id, int action);
    void on_resize(Player self, int size_x, int size_y);

    void update(Player self, float dt);
    void get_camera(Player self, Camera *camera);
} // namespace player
