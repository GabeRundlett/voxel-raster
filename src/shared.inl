#pragma once

#include <daxa/utils/task_graph.inl>

struct MyVertex {
    daxa_f32vec3 position;
    daxa_f32vec3 color;
};

DAXA_DECL_BUFFER_PTR(MyVertex)

DAXA_DECL_TASK_HEAD_BEGIN(DrawToSwapchainH)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, color_target)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(MyVertex), vertices)
DAXA_DECL_TASK_HEAD_END

struct MyPushConstant {
    DAXA_TH_BLOB(DrawToSwapchainH, attachments)
};
