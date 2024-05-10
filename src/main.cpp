#include <daxa/daxa.hpp>

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
    bool swapchain_out_of_date = false;
};

struct DrawToSwapchainTask : DrawToSwapchainH::Task {
    AttachmentViews views = {};
    daxa::RasterPipeline *pipeline = {};
    void callback(daxa::TaskInterface ti) {
        daxa::TaskImageAttachmentInfo const &image_attach_info = ti.get(AT.color_target);
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
        MyPushConstant push = {};
        ti.assign_attachment_shader_blob(push.attachments.value);
        render_recorder.push_constant(push);
        render_recorder.draw_mesh_tasks(1, 1, 1);
        ti.recorder = std::move(render_recorder).end_renderpass();
    }
};

auto main() -> int {
    auto window_info = WindowInfo{.width = 800, .height = 600};
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto *glfw_window_ptr = glfwCreateWindow(
        static_cast<daxa::i32>(window_info.width),
        static_cast<daxa::i32>(window_info.height),
        "voxel-raster", nullptr, nullptr);
    glfwSetWindowUserPointer(glfw_window_ptr, &window_info);
    glfwSetWindowSizeCallback(
        glfw_window_ptr,
        [](GLFWwindow *glfw_window, int width, int height) {
            auto &window_info_ref = *reinterpret_cast<WindowInfo *>(glfwGetWindowUserPointer(glfw_window));
            window_info_ref.swapchain_out_of_date = true;
            window_info_ref.width = static_cast<daxa::u32>(width);
            window_info_ref.height = static_cast<daxa::u32>(height);
        });
    auto *native_window_handle = get_native_handle(glfw_window_ptr);
    auto native_window_platform = get_native_platform(glfw_window_ptr);
    daxa::Instance instance = daxa::create_instance({});
    daxa::Device device = instance.create_device({
        .flags = daxa::DeviceFlags2{
            .mesh_shader_bit = true,
        },
        .name = "my device",
    });
    daxa::Swapchain swapchain = device.create_swapchain({
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
    daxa::ImGuiRenderer imgui_renderer = daxa::ImGuiRenderer({
        .device = device,
        .format = swapchain.get_format(),
        .context = ImGui::GetCurrentContext(),
        .use_custom_config = false,
    });
    ImGui_ImplGlfw_InitForVulkan(glfw_window_ptr, true);
    auto pipeline_manager = daxa::PipelineManager({
        .device = device,
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
    std::shared_ptr<daxa::RasterPipeline> pipeline;
    {
        auto result = pipeline_manager.add_raster_pipeline({
            .mesh_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .task_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .color_attachments = {{.format = swapchain.get_format()}},
            .raster = {},
            .push_constant_size = sizeof(MyPushConstant),
            .name = "my pipeline",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            return -1;
        }
        pipeline = result.value();
    }
    auto task_swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};
    auto loop_task_graph = daxa::TaskGraph({
        .device = device,
        .swapchain = swapchain,
        .name = "loop",
    });
    loop_task_graph.use_persistent_image(task_swapchain_image);
    loop_task_graph.add_task(DrawToSwapchainTask{
        .views = std::array{
            daxa::attachment_view(DrawToSwapchainH::AT.color_target, task_swapchain_image),
            daxa::attachment_view(DrawToSwapchainH::AT.temp, daxa::NullTaskBuffer),
        },
        .pipeline = pipeline.get(),
    });
    loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, daxa::ImageViewType::REGULAR_2D, task_swapchain_image),
        },
        .task = [&](daxa::TaskInterface const &ti) {
            auto swapchain_image = task_swapchain_image.get_state().images[0];
            imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, swapchain_image, window_info.width, window_info.height);
        },
        .name = "ImGui draw",
    });
    loop_task_graph.submit({});
    loop_task_graph.present({});
    loop_task_graph.complete({});
    while (true) {
        glfwPollEvents();
        if (glfwWindowShouldClose(glfw_window_ptr) != 0) {
            break;
        }
        if (window_info.swapchain_out_of_date) {
            swapchain.resize();
            window_info.swapchain_out_of_date = false;
        }
        auto swapchain_image = swapchain.acquire_next_image();
        if (swapchain_image.is_empty()) {
            continue;
        }
        auto reload_result = pipeline_manager.reload_all();
        if (auto *reload_err = daxa::get_if<daxa::PipelineReloadError>(&reload_result)) {
            std::cout << reload_err->message << std::endl;
        }
        task_swapchain_image.set_images({.images = std::span{&swapchain_image, 1}});
        {
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::ShowMetricsWindow(nullptr);
            ImGui::Render();
        }
        loop_task_graph.execute({});
        device.collect_garbage();
    }
    device.wait_idle();
    device.collect_garbage();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}