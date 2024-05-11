#include <chrono>
#include <daxa/daxa.hpp>

#include <daxa/pipeline.hpp>
#include <daxa/types.hpp>
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/imgui.hpp>
#include <imgui.h>
#include <iostream>
#include <imgui_impl_glfw.h>

#include "shared.inl"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include "player.hpp"

auto get_native_handle(GLFWwindow *glfw_window_ptr) -> daxa::NativeWindowHandle {
#if defined(_WIN32)
    return glfwGetWin32Window(glfw_window_ptr);
#elif defined(__linux__)
    return reinterpret_cast<daxa::NativeWindowHandle>(glfwGetX11Window(glfw_window_ptr));
#endif
}

auto get_native_platform(GLFWwindow * /*unused*/) -> daxa::NativeWindowPlatform {
#if defined(_WIN32)
    return daxa::NativeWindowPlatform::WIN32_API;
#elif defined(__linux__)
    return daxa::NativeWindowPlatform::XLIB_API;
#endif
}

struct WindowInfo {
    daxa::u32 width{}, height{};
};

struct DrawVisbufferTask : DrawVisbufferH::Task {
    AttachmentViews views = {};
    daxa::RasterPipeline *pipeline = {};
    void callback(daxa::TaskInterface ti) {
        daxa::TaskImageAttachmentInfo const &image_attach_info = ti.get(AT.render_target);
        daxa::ImageInfo image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
            .color_attachments = std::array{
                daxa::RenderAttachmentInfo{
                    .image_view = image_attach_info.view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<daxa::u32, 4>{0, 0, 0, 0},
                },
            },
            .depth_attachment = daxa::RenderAttachmentInfo{
                .image_view = ti.get(AT.depth_target).view_ids[0],
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue{.depth = 0.0f},
            },
            .render_area = {.width = image_info.size.x, .height = image_info.size.y},
        });
        render_recorder.set_pipeline(*pipeline);
        DrawVisbufferPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        render_recorder.push_constant(push);
        render_recorder.draw_mesh_tasks(1, 1, 1);
        ti.recorder = std::move(render_recorder).end_renderpass();
    }
};

struct ShadeVisbufferTask : ShadeVisbufferH::Task {
    AttachmentViews views = {};
    daxa::RasterPipeline *pipeline = {};
    void callback(daxa::TaskInterface ti) {
        daxa::TaskImageAttachmentInfo const &image_attach_info = ti.get(AT.render_target);
        daxa::ImageInfo image_info = ti.device.info_image(image_attach_info.ids[0]).value();
        daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
            .color_attachments = std::array{
                daxa::RenderAttachmentInfo{
                    .image_view = image_attach_info.view_ids[0],
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<daxa::f32, 4>{0.1f, 0.0f, 0.5f, 1.0f},
                },
            },
            .render_area = {.width = image_info.size.x, .height = image_info.size.y},
        });
        render_recorder.set_pipeline(*pipeline);
        ShadeVisbufferPush push = {};
        ti.assign_attachment_shader_blob(push.uses.value);
        render_recorder.push_constant(push);
        render_recorder.draw({.vertex_count = 3});
        ti.recorder = std::move(render_recorder).end_renderpass();
    }
};

using Clock = std::chrono::steady_clock;

struct AppState {
    WindowInfo window_info;
    bool paused;
    player::Player player;

    GLFWwindow *glfw_window_ptr;

    daxa::Instance instance;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::ImGuiRenderer imgui_renderer;
    daxa::PipelineManager pipeline_manager;

    std::shared_ptr<daxa::RasterPipeline> draw_visbuffer_pipeline;
    std::shared_ptr<daxa::RasterPipeline> shade_visbuffer_pipeline;
    daxa::TaskImage task_swapchain_image;
    daxa::TaskGraph loop_task_graph;

    Clock::time_point prev_time;
};

template <typename T>
inline void allocate_fill_copy(daxa::TaskInterface ti, T value, daxa::TaskBufferAttachmentInfo dst, uint32_t dst_offset = 0) {
    auto alloc = ti.allocator->allocate_fill(value).value();
    ti.recorder.copy_buffer_to_buffer({
        .src_buffer = ti.allocator->buffer(),
        .dst_buffer = dst.ids[0],
        .src_offset = alloc.buffer_offset,
        .dst_offset = dst_offset,
        .size = sizeof(T),
    });
}

void record_tasks(AppState &self) {
    self.loop_task_graph = daxa::TaskGraph({
        .device = self.device,
        .swapchain = self.swapchain,
        .name = "loop",
    });
    self.loop_task_graph.use_persistent_image(self.task_swapchain_image);
    auto task_input_buffer = self.loop_task_graph.create_transient_buffer({
        .size = sizeof(GpuInput),
        .name = "gpu_input",
    });
    self.loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_input_buffer),
        },
        .task = [self, task_input_buffer](daxa::TaskInterface const &ti) {
            auto gpu_input = GpuInput{};
            gpu_input.render_size = daxa_u32vec2{self.window_info.width, self.window_info.height};
            player::get_camera(self.player, &gpu_input.cam);
            allocate_fill_copy(ti, gpu_input, ti.get(task_input_buffer));
        },
        .name = "GpuInputUploadTransferTask",
    });
    auto task_visbuffer = self.loop_task_graph.create_transient_image({
        .format = daxa::Format::R32_UINT,
        .size = {self.window_info.width, self.window_info.height, 1},
        .name = "visbuffer",
    });
    auto task_depth = self.loop_task_graph.create_transient_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {self.window_info.width, self.window_info.height, 1},
        .name = "depth",
    });
    self.loop_task_graph.add_task(DrawVisbufferTask{
        .views = std::array{
            daxa::attachment_view(DrawVisbufferH::AT.render_target, task_visbuffer),
            daxa::attachment_view(DrawVisbufferH::AT.depth_target, task_depth),
            daxa::attachment_view(DrawVisbufferH::AT.gpu_input, task_input_buffer),
        },
        .pipeline = self.draw_visbuffer_pipeline.get(),
    });
    self.loop_task_graph.add_task(ShadeVisbufferTask{
        .views = std::array{
            daxa::attachment_view(ShadeVisbufferH::AT.render_target, self.task_swapchain_image),
            daxa::attachment_view(ShadeVisbufferH::AT.gpu_input, task_input_buffer),
            daxa::attachment_view(ShadeVisbufferH::AT.visbuffer, task_visbuffer),
        },
        .pipeline = self.shade_visbuffer_pipeline.get(),
    });
    self.loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, daxa::ImageViewType::REGULAR_2D, self.task_swapchain_image),
        },
        .task = [&](daxa::TaskInterface const &ti) {
            auto swapchain_image = self.task_swapchain_image.get_state().images[0];
            self.imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, swapchain_image, self.window_info.width, self.window_info.height);
        },
        .name = "ImGui draw",
    });
    self.loop_task_graph.submit({});
    self.loop_task_graph.present({});
    self.loop_task_graph.complete({});
}

void on_resize(AppState &self) {
    self.swapchain.resize();
    record_tasks(self);
    player::on_resize(self.player, self.window_info.width, self.window_info.height);
}

void init(AppState &self) {
    self.window_info = {.width = 800, .height = 600};
    self.paused = true;
    self.player = {};

    player::init(self.player);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    self.glfw_window_ptr = glfwCreateWindow(
        static_cast<daxa::i32>(self.window_info.width),
        static_cast<daxa::i32>(self.window_info.height),
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
                daxa_f32vec2 const center = {float(self.window_info.width / 2), float(self.window_info.height / 2)};
                auto offset = daxa_f32vec2{float(x) - center.x, float(y) - center.y};
                player::on_mouse_move(self.player, offset.x, offset.y);
                glfwSetCursorPos(glfw_window, double(center.x), double(center.y));
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
                player::on_key(self.player, key, action);
            }
        });
    auto *native_window_handle = get_native_handle(self.glfw_window_ptr);
    auto native_window_platform = get_native_platform(self.glfw_window_ptr);
    self.instance = daxa::create_instance({});
    self.device = self.instance.create_device({
        .flags = daxa::DeviceFlags2{
            .mesh_shader_bit = true,
        },
        .name = "my device",
    });
    self.swapchain = self.device.create_swapchain({
        .native_window = native_window_handle,
        .native_window_platform = native_window_platform,
        .surface_format_selector = [](daxa::Format format) {
            switch (format) {
            case daxa::Format::R8G8B8A8_UINT: return 100;
            default: return daxa::default_format_score(format);
            }
        },
        .present_mode = daxa::PresentMode::MAILBOX,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .name = "my swapchain",
    });
    ImGui::CreateContext();
    self.imgui_renderer = daxa::ImGuiRenderer({
        .device = self.device,
        .format = self.swapchain.get_format(),
        .context = ImGui::GetCurrentContext(),
        .use_custom_config = false,
    });
    ImGui_ImplGlfw_InitForVulkan(self.glfw_window_ptr, true);
    self.pipeline_manager = daxa::PipelineManager({
        .device = self.device,
        .shader_compile_options = {
            .root_paths = {
                DAXA_SHADER_INCLUDE_DIR,
                "src",
            },
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true,
        },
        .name = "my pipeline manager",
    });

    {
        auto result = self.pipeline_manager.add_raster_pipeline({
            .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}},
            .task_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"draw_visbuffer.glsl"}},
            .color_attachments = {{.format = daxa::Format::R32_UINT}},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER,
            },
            .raster = {},
            .push_constant_size = sizeof(DrawVisbufferPush),
            .name = "my pipeline",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self.draw_visbuffer_pipeline = result.value();
    }

    {
        auto result = self.pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"shade_visbuffer.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"shade_visbuffer.glsl"}},
            .color_attachments = {{.format = self.swapchain.get_format()}},
            .raster = {},
            .push_constant_size = sizeof(ShadeVisbufferPush),
            .name = "my pipeline",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            std::terminate();
        }
        self.shade_visbuffer_pipeline = result.value();
    }

    self.task_swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};

    on_resize(self);

    self.prev_time = Clock::now();
}

void deinit(AppState &self) {
    self.device.wait_idle();
    self.device.collect_garbage();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    player::deinit(self.player);
}

void draw(AppState &self) {
    auto swapchain_image = self.swapchain.acquire_next_image();
    if (swapchain_image.is_empty()) {
        return;
    }
    auto reload_result = self.pipeline_manager.reload_all();
    if (auto *reload_err = daxa::get_if<daxa::PipelineReloadError>(&reload_result)) {
        std::cout << reload_err->message << std::endl;
    }
    self.task_swapchain_image.set_images({.images = std::span{&swapchain_image, 1}});
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowMetricsWindow(nullptr);
        ImGui::Render();
    }
    self.loop_task_graph.execute({});
    self.device.collect_garbage();
}

auto update(AppState &self) -> bool {
    glfwPollEvents();
    if (glfwWindowShouldClose(self.glfw_window_ptr) != 0) {
        return false;
    }
    auto now = Clock::now();
    auto dt = std::chrono::duration<float>(now - self.prev_time).count();
    self.prev_time = now;
    player::update(self.player, dt);
    draw(self);
    return true;
}

auto main() -> int {
    AppState app;
    init(app);
    while (update(app)) {
    }
    deinit(app);
}
