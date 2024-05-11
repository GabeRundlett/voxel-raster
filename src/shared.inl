#pragma once

#include <daxa/utils/task_graph.inl>
#include "camera.inl"

struct GpuInput {
    daxa_u32vec2 render_size;
    Camera cam;
};
DAXA_DECL_BUFFER_PTR(GpuInput)

DAXA_DECL_TASK_HEAD_BEGIN(DrawVisbufferH)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_DECL_TASK_HEAD_END

struct DrawVisbufferPush {
    DAXA_TH_BLOB(DrawVisbufferH, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(ShadeVisbufferH)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, render_target)
DAXA_TH_BUFFER_PTR(FRAGMENT_SHADER_READ, daxa_BufferPtr(GpuInput), gpu_input)
DAXA_TH_IMAGE_ID(FRAGMENT_SHADER_SAMPLED, REGULAR_2D, visbuffer)
DAXA_DECL_TASK_HEAD_END

struct ShadeVisbufferPush {
    DAXA_TH_BLOB(ShadeVisbufferH, uses)
};
