#pragma once

namespace debug_utils {
    struct ConsoleState;
    using Console = ConsoleState *;

    void init(Console &self);
    void deinit(Console self);

    void clear_log(Console self);
    void add_log(Console self, char const *str);
    void draw_imgui(Console self, const char *title, bool *p_open);
} // namespace debug_utils

extern debug_utils::Console g_console;
