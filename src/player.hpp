#pragma once

namespace player {
    struct State;
    using Player = State *;

    void init(Player &self);
    void deinit(Player self);
    void on_key(Player self, int key, int action, int mods);

    void on_mouse_move(Player self, float x, float y);
    void on_key(Player self, int key_id, int action);

    void update(Player self, float dt);
} // namespace player
