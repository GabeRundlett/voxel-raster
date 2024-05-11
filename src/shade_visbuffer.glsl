#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(ShadeVisbufferPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

void main() {
    vec2 uv = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1);
    gl_Position = vec4(uv * 4 - 1, 0, 1);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) out vec4 f_out;

void main() {
    uint visbuffer_id = texelFetch(daxa_utexture2D(push.uses.visbuffer), ivec2(gl_FragCoord.xy), 0).x;
    f_out = vec4(float(visbuffer_id & 7) / 7, float((visbuffer_id >> 3) & 7) / 7, float((visbuffer_id >> 6) & 7) / 7, 1);
}

#endif
