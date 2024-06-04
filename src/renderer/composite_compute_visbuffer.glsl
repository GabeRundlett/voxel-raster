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

layout(location = 0) out uvec4 f_out;

void main() {
    uint64_t visbuffer_id64 = imageLoad(daxa_u64image2D(push.uses.visbuffer64), ivec2(gl_FragCoord.xy)).x;
    float depth = uintBitsToFloat(uint(visbuffer_id64 >> uint64_t(32)));
    uint visbuffer_id = uint(visbuffer_id64);

    gl_FragDepth = depth;
    f_out = uvec4(visbuffer_id, 0, 0, 0);
}

#endif
