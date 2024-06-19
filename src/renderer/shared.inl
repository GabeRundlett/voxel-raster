#pragma once

#define DAXA_IMAGE_INT64 1

#include <daxa/utils/task_graph.inl>
#include "camera.inl"
#include <voxels/voxel_mesh.inl>

#define ENABLE_DEBUG_VIS 1

struct DispatchIndirectStruct {
    daxa_u32 x;
    daxa_u32 y;
    daxa_u32 z;
    daxa_u32 offset;
};
DAXA_DECL_BUFFER_PTR(DispatchIndirectStruct)

struct GpuInput {
    daxa_u32vec2 render_size;
    daxa_u32vec2 next_lower_po2_render_size;
    daxa_u32 chunk_n;
    daxa_u32 frame_index;
    daxa_f32 time;
    daxa_f32 delta_time;
    daxa_f32vec2 jitter;
    Camera cam;
    Camera observer_cam;
};
DAXA_DECL_BUFFER_PTR(GpuInput)

DAXA_DECL_TASK_HEAD_BEGIN(PostProcessing)
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
DAXA_TH_IMAGE_INDEX(FRAGMENT_SHADER_SAMPLED, REGULAR_2D, color)
DAXA_DECL_TASK_HEAD_END

struct PostProcessingPush {
    DAXA_TH_BLOB(PostProcessing, uses)
};

