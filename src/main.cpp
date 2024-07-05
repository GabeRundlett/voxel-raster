#include "renderer/renderer.hpp"
#include <chrono>

#include <imgui.h>

#include "player.hpp"
#include "audio.hpp"
#include "renderer/renderer.hpp"
#include "voxels/voxel_world.hpp"
#include "utilities/debug.hpp"

#include <GLFW/glfw3.h>

struct WindowInfo {
    uint32_t width{}, height{};
};

using Clock = std::chrono::steady_clock;

renderer::Renderer g_renderer;
voxel_world::VoxelWorld g_voxel_world;
debug_utils::Console g_console;

struct AppState {
    WindowInfo window_info;
    bool paused;
    player::Player player;
    renderer::Renderer renderer;
    voxel_world::VoxelWorld voxel_world;

    GLFWwindow *glfw_window_ptr;
    Clock::time_point prev_time;
    Clock::time_point start_time;
};

void on_resize(AppState &self) {
    on_resize(self.player, self.window_info.width, self.window_info.height);
    on_resize(self.renderer, self.window_info.width, self.window_info.height);
    draw(self.renderer, self.player, self.voxel_world);
}

void init(AppState &self) {
    self.window_info = {.width = 800, .height = 600};
    self.paused = true;
    init(self.player);

    init(self.voxel_world);
    g_voxel_world = self.voxel_world;

    audio::init();
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    self.glfw_window_ptr = glfwCreateWindow(
        static_cast<int>(self.window_info.width),
        static_cast<int>(self.window_info.height),
        "voxel-raster", nullptr, nullptr);
    glfwSetWindowUserPointer(self.glfw_window_ptr, &self);
    glfwSetWindowSizeCallback(
        self.glfw_window_ptr,
        [](GLFWwindow *glfw_window, int width, int height) {
            auto &self = *reinterpret_cast<AppState *>(glfwGetWindowUserPointer(glfw_window));
            self.window_info.width = uint32_t(width);
            self.window_info.height = uint32_t(height);
            on_resize(self);
        });
    glfwSetCursorPosCallback(
        self.glfw_window_ptr,
        [](GLFWwindow *glfw_window, double x, double y) {
            auto &self = *reinterpret_cast<AppState *>(glfwGetWindowUserPointer(glfw_window));
            if (!self.paused) {
                auto const center_x = float(self.window_info.width / 2);
                auto const center_y = float(self.window_info.height / 2);
                auto const offset_x = float(x) - center_x;
                auto const offset_y = float(y) - center_y;
                on_mouse_move(self.player, offset_x, offset_y);
                glfwSetCursorPos(glfw_window, double(center_x), double(center_y));
            }
        });
    glfwSetScrollCallback(
        self.glfw_window_ptr,
        [](GLFWwindow *glfw_window, double x, double y) {
            auto &self = *reinterpret_cast<AppState *>(glfwGetWindowUserPointer(glfw_window));
            if (!self.paused) {
                on_mouse_scroll(self.player, x, y);
            }
        });
    glfwSetMouseButtonCallback(
        self.glfw_window_ptr,
        [](GLFWwindow *glfw_window, int button_id, int action, int) {
            auto &self = *reinterpret_cast<AppState *>(glfwGetWindowUserPointer(glfw_window));

            if (!self.paused) {
                on_mouse_button(self.player, button_id, action);
            }
        });
    glfwSetKeyCallback(
        self.glfw_window_ptr,
        [](GLFWwindow *glfw_window, int key, int, int action, int) {
            auto &self = *reinterpret_cast<AppState *>(glfwGetWindowUserPointer(glfw_window));

            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                auto should_capture = self.paused;
                self.paused = !self.paused;
                glfwSetCursorPos(glfw_window, double(self.window_info.width / 2), double(self.window_info.height / 2));
                glfwSetInputMode(glfw_window, GLFW_CURSOR, should_capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                glfwSetInputMode(glfw_window, GLFW_RAW_MOUSE_MOTION, should_capture);
            }

            if (!self.paused) {
                on_key(self.player, key, action);
            }
            if (key == GLFW_KEY_V && action == GLFW_PRESS) {
                toggle_vsync(self.renderer);
            }
            if (key == GLFW_KEY_L && action == GLFW_PRESS) {
                toggle_fsr2(self.renderer);
            }
            if (key == GLFW_KEY_R && action == GLFW_PRESS) {
                toggle_rt(self.renderer);
            }
            if (key == GLFW_KEY_K && action == GLFW_PRESS) {
                toggle_shadows(self.renderer);
            }
        });

    init(self.renderer, self.glfw_window_ptr);
    g_renderer = self.renderer;

    self.prev_time = Clock::now();
    self.start_time = Clock::now();
    on_resize(self);
}

void deinit(AppState &self) {
    deinit(self.player);
    deinit(self.voxel_world);
    deinit(self.renderer);
    audio::deinit();
}

auto update(AppState &self) -> bool {
    glfwPollEvents();
    if (glfwWindowShouldClose(self.glfw_window_ptr) != 0) {
        return false;
    }
    auto now = Clock::now();
    auto dt = std::chrono::duration<float>(now - self.prev_time).count();
    auto time = std::chrono::duration<float>(now - self.start_time).count();
    self.prev_time = now;
    update(self.player, dt);
    update(self.voxel_world);
    draw(self.renderer, self.player, self.voxel_world);
    return true;
}

auto main() -> int {
    init(g_console);

    AppState app;
    init(app);
    while (update(app)) {
    }
    deinit(app);

    deinit(g_console);
}
