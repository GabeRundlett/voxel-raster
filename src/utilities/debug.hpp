#pragma once

struct Console;

namespace debug_utils {
    auto create_console() -> Console *;
    void destroy(Console *self);

    void clear_log(Console *self);
    void add_log(Console *self, char const *str);
    void draw_imgui(Console *self, const char *title, bool *p_open);
} // namespace debug_utils

extern Console *g_console;
