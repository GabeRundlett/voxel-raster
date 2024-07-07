#include "gpu_context.hpp"

#include <fmt/format.h>
#include <renderer/shared.inl>
#include <cassert>

GpuContext::GpuContext() {
    daxa_instance = daxa::create_instance({});
    device = daxa_instance.create_device({
        .flags = daxa::DeviceFlags2{
            // .buffer_device_address_capture_replay_bit = false,
            // .conservative_rasterization = true,
            .mesh_shader_bit = true,
            .image_atomic64 = true,
            .ray_tracing = true,
            // .shader_float16 = true,
            // .robust_buffer_access = true,
            // .robust_image_access = true,
        },
        .max_allowed_buffers = MAX_CHUNK_COUNT + 1000,
        .name = "device",
    });
    pipeline_manager = std::make_shared<AsyncPipelineManager>(daxa::PipelineManagerInfo{
        .device = device,
        .shader_compile_options = {
            .root_paths = {
                DAXA_SHADER_INCLUDE_DIR,
                "src",
                "src/renderer",
            },
            // .write_out_preprocessed_code = ".out/",
            .write_out_shader_binary = ".out/spv",
            // .spirv_cache_folder = ".out/spv_cache",
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true,
        },
        .register_null_pipelines_when_first_compile_fails = true,
        .name = "pipeline_manager",
    });

    pipeline_manager->add_virtual_file({
        .name = "FULL_SCREEN_TRIANGLE_VERTEX_SHADER",
        .contents = R"glsl(
            void main() {
                vec2 uv = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1);
                gl_Position = vec4(uv * 4 - 1, 0, 1);
            }
        )glsl",
    });

    pipeline_manager->add_virtual_file({
        .name = "R32_D32_BLIT",
        .contents = R"glsl(
            #include <renderer/trace_primary.inl>
            DAXA_DECL_PUSH_CONSTANT(R32D32BlitPush, push)
            daxa_ImageViewIndex input_tex = push.uses.input_tex;
            void main() {
                gl_FragDepth = texelFetch(daxa_texture2D(input_tex), ivec2(gl_FragCoord.xy), 0).r;
            }
        )glsl",
    });

    input_buffer = device.create_buffer({
        .size = sizeof(GpuInput),
        .name = "input_buffer",
    });
    sampler_nnc = device.create_sampler({
        .magnification_filter = daxa::Filter::NEAREST,
        .minification_filter = daxa::Filter::NEAREST,
        .max_lod = 0.0f,
    });
    sampler_lnc = device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::NEAREST,
        .max_lod = 0.0f,
    });
    sampler_llc = device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .max_lod = 0.0f,
    });
    sampler_llr = device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .address_mode_u = daxa::SamplerAddressMode::REPEAT,
        .address_mode_v = daxa::SamplerAddressMode::REPEAT,
        .address_mode_w = daxa::SamplerAddressMode::REPEAT,
        .max_lod = 0.0f,
    });

    auto g_samplers_header = daxa::VirtualFileInfo{
        .name = "g_samplers",
        .contents = "#pragma once\n#include <daxa/daxa.glsl>\n",
    };
    g_samplers_header.contents += "daxa_SamplerId g_sampler_nnc = daxa_SamplerId(" + std::to_string(std::bit_cast<uint64_t>(sampler_nnc)) + ");\n";
    g_samplers_header.contents += "daxa_SamplerId g_sampler_lnc = daxa_SamplerId(" + std::to_string(std::bit_cast<uint64_t>(sampler_lnc)) + ");\n";
    g_samplers_header.contents += "daxa_SamplerId g_sampler_llc = daxa_SamplerId(" + std::to_string(std::bit_cast<uint64_t>(sampler_llc)) + ");\n";
    g_samplers_header.contents += "daxa_SamplerId g_sampler_llr = daxa_SamplerId(" + std::to_string(std::bit_cast<uint64_t>(sampler_llr)) + ");\n";
    pipeline_manager->add_virtual_file(g_samplers_header);

    task_input_buffer.set_buffers({.buffers = std::array{input_buffer}});
}

GpuContext::~GpuContext() {
    device.destroy_buffer(input_buffer);
    device.destroy_sampler(sampler_nnc);
    device.destroy_sampler(sampler_lnc);
    device.destroy_sampler(sampler_llc);
    device.destroy_sampler(sampler_llr);

    for (auto const &[id, temporal_buffer] : temporal_buffers) {
        device.destroy_buffer(temporal_buffer.resource_id);
    }
    for (auto const &[id, temporal_image] : temporal_images) {
        device.destroy_image(temporal_image.resource_id);
    }
}

void GpuContext::create_swapchain(daxa::SwapchainInfo const &info) {
    swapchain = device.create_swapchain(info);
}

auto GpuContext::find_or_add_temporal_buffer(daxa::BufferInfo const &info) -> TemporalBuffer {
    auto id = std::string{info.name.view()};
    auto iter = temporal_buffers.find(id);

    if (iter == temporal_buffers.end()) {
        auto result = TemporalBuffer{};
        result.resource_id = device.create_buffer(info);
        result.task_resource = daxa::TaskBuffer(daxa::TaskBufferInfo{.initial_buffers = {.buffers = std::array{result.resource_id}}, .name = id});
        auto emplace_result = temporal_buffers.emplace(id, result);
        iter = emplace_result.first;
    } else {
        auto existing_info = device.info_buffer(iter->second.resource_id).value();
        if (existing_info.size != info.size) {
            add_log(g_console, fmt::format("TemporalBuffer \"{}\" recreated with bad size... This should NEVER happen!!!", id).c_str());
        }
    }

    return iter->second;
}

auto GpuContext::find_or_add_temporal_image(daxa::ImageInfo const &info) -> TemporalImage {
    auto id = std::string{info.name.view()};
    auto iter = temporal_images.find(id);

    if (iter == temporal_images.end()) {
        auto result = TemporalImage{};
        result.resource_id = device.create_image(info);
        result.task_resource = daxa::TaskImage(daxa::TaskImageInfo{.initial_images = {.images = std::array{result.resource_id}}, .name = id});
        auto emplace_result = temporal_images.emplace(id, result);
        iter = emplace_result.first;
    } else {
        auto existing_info = device.info_image(iter->second.resource_id).value();
        if (existing_info.size != info.size) {
            add_log(g_console, fmt::format("TemporalImage \"{}\" recreated with bad size... This should NEVER happen!!!", id).c_str());
        }
    }

    return iter->second;
}

void GpuContext::remove_temporal_buffer(std::string const &id) {
    auto iter = temporal_buffers.find(id);
    if (iter == temporal_buffers.end()) {
        return;
    }
    // assert(iter != temporal_buffers.end());
    device.destroy_buffer(iter->second.resource_id);
    temporal_buffers.erase(iter);
}

void GpuContext::remove_temporal_image(std::string const &id) {
    auto iter = temporal_images.find(id);
    if (iter == temporal_images.end()) {
        return;
    }
    // assert(iter != temporal_images.end());
    device.destroy_image(iter->second.resource_id);
    temporal_images.erase(iter);
}

void GpuContext::remove_temporal_buffer(daxa::BufferId id) {
    auto res_info = device.info_buffer(id);
    if (!res_info.has_value()) {
        return;
    }
    // assert(res_info.has_value());
    remove_temporal_buffer(std::string{res_info.value().name.view()});
}

void GpuContext::remove_temporal_image(daxa::ImageId id) {
    auto res_info = device.info_image(id);
    if (!res_info.has_value()) {
        return;
    }
    // assert(res_info.has_value());
    remove_temporal_image(std::string{res_info.value().name.view()});
}

auto GpuContext::resize_temporal_buffer(std::string const &s_id, uint64_t new_size) -> TemporalBuffer {
    auto iter = temporal_buffers.find(s_id);
    assert(iter != temporal_buffers.end());
    auto info = daxa::BufferInfo{device.info_buffer(iter->second.resource_id).value()};
    if (info.size != new_size) {
        info.size = new_size;
        device.destroy_buffer(iter->second.resource_id);
        iter->second.resource_id = device.create_buffer(info);
        iter->second.task_resource.set_buffers({.buffers = std::array{iter->second.resource_id}});
    }
    return iter->second;
}

auto GpuContext::resize_temporal_image(std::string const &s_id, daxa::Extent3D new_size) -> TemporalImage {
    auto iter = temporal_images.find(s_id);
    assert(iter != temporal_images.end());
    auto info = daxa::ImageInfo{device.info_image(iter->second.resource_id).value()};
    if (info.size != new_size) {
        info.size = new_size;
        device.destroy_image(iter->second.resource_id);
        iter->second.resource_id = device.create_image(info);
        iter->second.task_resource.set_images({.images = std::array{iter->second.resource_id}});
    }
    return iter->second;
}

auto GpuContext::resize_temporal_buffer(daxa::BufferId id, uint64_t new_size) -> TemporalBuffer {
    auto res_info = device.info_buffer(id);
    assert(res_info.has_value());
    auto info = res_info.value();
    auto iter = temporal_buffers.find(std::string(info.name.view()));
    assert(iter != temporal_buffers.end());
    if (info.size != new_size) {
        info.size = new_size;
        device.destroy_buffer(iter->second.resource_id);
        iter->second.resource_id = device.create_buffer(info);
        iter->second.task_resource.set_buffers({.buffers = std::array{iter->second.resource_id}});
    }
    return iter->second;
}

auto GpuContext::resize_temporal_image(daxa::ImageId id, daxa::Extent3D new_size) -> TemporalImage {
    auto res_info = device.info_image(id);
    assert(res_info.has_value());
    auto info = res_info.value();
    auto iter = temporal_images.find(std::string(info.name.view()));
    assert(iter != temporal_images.end());
    if (info.size != new_size) {
        info.size = new_size;
        device.destroy_image(iter->second.resource_id);
        iter->second.resource_id = device.create_image(info);
        iter->second.task_resource.set_images({.images = std::array{iter->second.resource_id}});
    }
    return iter->second;
}
