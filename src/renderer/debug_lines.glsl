#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(DebugLinesPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

void main() {
    mat4 world_to_clip = deref(push.uses.gpu_input).observer_cam.view_to_clip * deref(push.uses.gpu_input).observer_cam.world_to_view;
    gl_Position = world_to_clip * vec4(deref(push.line_points[gl_VertexIndex]), 1);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) out vec4 f_out;

void main() {
    f_out = vec4(1);
}

#endif
