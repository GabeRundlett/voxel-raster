#pragma once

#include <daxa/utils/task_graph.inl>

DAXA_DECL_TASK_HEAD_BEGIN(DrawToSwapchainH)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, color_target)
DAXA_TH_BUFFER_PTR(MESH_SHADER_READ, daxa_BufferPtr(uint), temp)
DAXA_DECL_TASK_HEAD_END

struct MyPushConstant {
    DAXA_TH_BLOB(DrawToSwapchainH, attachments)
};
