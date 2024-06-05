#pragma once

#include <daxa/utils/task_graph.inl>

struct Camera {
    daxa_f32mat4x4 view_to_clip;
    daxa_f32mat4x4 clip_to_view;
    daxa_f32mat4x4 view_to_sample;
    daxa_f32mat4x4 sample_to_view;
    daxa_f32mat4x4 world_to_view;
    daxa_f32mat4x4 view_to_world;
    daxa_f32mat4x4 clip_to_prev_clip;
    daxa_f32mat4x4 prev_view_to_prev_clip;
    daxa_f32mat4x4 prev_clip_to_prev_view;
    daxa_f32mat4x4 prev_world_to_prev_view;
    daxa_f32mat4x4 prev_view_to_prev_world;
};
