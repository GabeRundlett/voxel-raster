#include <shared.inl>
#include <renderer/visbuffer.glsl>
#include <voxels/pack_unpack.inl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/meshlet_allocator.glsl>

DAXA_DECL_PUSH_CONSTANT(CompositeComputeVisbufferPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

void main() {
    vec2 uv = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1);
    gl_Position = vec4(uv * 4 - 1, 0, 1);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

void main() {
    uint visbuffer_id64 = uint(imageLoad(daxa_u64image2D(push.uses.visbuffer64), ivec2(gl_FragCoord.xy)).x);
}

#endif
