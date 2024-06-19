#pragma once

#include <daxa/utils/task_graph.hpp>

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

template <typename T>
inline void task_fill_buffer(daxa::TaskGraph &tg, daxa::TaskBufferView buffer, T clear_value, uint32_t offset = 0) {
    tg.add_task({
        .attachments = {daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, buffer)},
        .task = [=](daxa::TaskInterface ti) { allocate_fill_copy(ti, clear_value, ti.get(buffer), offset); },
        .name = "fill buffer",
    });
}

template <size_t N>
inline void clear_task_buffers(daxa::TaskGraph &task_graph, std::array<daxa::TaskBufferView, N> const &task_buffer_views, std::array<daxa::BufferClearInfo, N> clear_infos = {}) {
    auto uses = std::vector<daxa::TaskAttachmentInfo>{};
    auto use_count = task_buffer_views.size();
    uses.reserve(use_count);
    for (auto const &task_buffer : task_buffer_views) {
        uses.push_back(daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_buffer));
    }
    task_graph.add_task({
        .attachments = std::move(uses),
        .task = [use_count, clear_infos](daxa::TaskInterface const &ti) mutable {
            for (uint8_t i = 0; i < use_count; ++i) {
                clear_infos[i].buffer = ti.get(daxa::TaskBufferAttachmentIndex{i}).ids[0];
                ti.recorder.clear_buffer(clear_infos[i]);
            }
        },
        .name = "clear buffers",
    });
}
template <size_t N>
inline void clear_task_images(daxa::TaskGraph &task_graph, std::array<daxa::TaskImageView, N> const &task_image_views, std::array<daxa::ClearValue, N> clear_values = {}) {
    auto uses = std::vector<daxa::TaskAttachmentInfo>{};
    auto use_count = task_image_views.size();
    uses.reserve(use_count);
    for (auto const &task_image : task_image_views) {
        uses.push_back(daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, daxa::ImageViewType::REGULAR_2D, task_image));
    }
    task_graph.add_task({
        .attachments = std::move(uses),
        .task = [use_count, clear_values](daxa::TaskInterface const &ti) {
            for (uint8_t i = 0; i < use_count; ++i) {
                ti.recorder.clear_image({
                    .dst_image_layout = ti.get(daxa::TaskImageAttachmentIndex{i}).layout,
                    .clear_value = clear_values[i],
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex{i}).ids[0],
                });
            }
        },
        .name = "clear images",
    });
}

inline constexpr auto round_up_div(auto x, auto y) {
    return (x + y - 1) / y;
}

inline constexpr auto find_msb(uint32_t v) -> uint32_t {
    uint32_t index = 0;
    while (v != 0) {
        v = v >> 1;
        index = index + 1;
    }
    return index;
}
inline constexpr auto find_next_lower_po2(uint32_t v) -> uint32_t {
    auto const msb = find_msb(v);
    return 1u << ((msb == 0 ? 1 : msb) - 1);
}
