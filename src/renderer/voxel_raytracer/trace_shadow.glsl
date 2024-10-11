#define DAXA_RAY_TRACING 1
#extension GL_EXT_ray_tracing : enable

#define VOXEL_RT_ANY_HIT 1

#include "voxel_raytracer.inl"
DAXA_DECL_PUSH_CONSTANT(TraceShadowPush, push)
#include "rt.glsl"

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_RAYGEN
#include "camera.glsl"
#include <voxels/voxel_mesh.glsl>

layout(location = PAYLOAD_LOC) rayPayloadEXT PackedRayPayload prd;

const vec3 SUN_DIR = normalize(vec3(-1.7, 2.4, 2.1));

void main() {
    const ivec2 px = ivec2(gl_LaunchIDEXT.xy);

    vec4 output_tex_size = vec4(deref(push.uses.gpu_input).render_size, 0, 0);
    output_tex_size.zw = vec2(1.0, 1.0) / output_tex_size.xy;
    vec2 uv = get_uv(gl_LaunchIDEXT.xy, output_tex_size);

    float depth = texelFetch(daxa_texture2D(push.uses.depth), px, 0).r;
    vec3 nrm = unmap_octahedral(texelFetch(daxa_texture2D(push.uses.normal), px, 0).xy);

    ViewRayContext vrc = vrc_from_uv_and_biased_depth(push.uses.gpu_input, uv, depth);
    vec3 ray_origin = biased_secondary_ray_origin_ws_with_normal(vrc, nrm);
    vec3 ray_pos = ray_origin;
    vec3 ray_dir = SUN_DIR;

    float hit = 0;
    if (depth != 0.0 && dot(nrm, ray_dir) > 0) {
        const uint ray_flags = gl_RayFlagsTerminateOnFirstHitEXT;
        const uint cull_mask = 0xFF;
        const uint sbt_record_offset = 0;
        const uint sbt_record_stride = 0;
        const uint miss_index = 0;
        const float t_min = 0.0001;
        const float t_max = 10000.0;

        traceRayEXT(
            accelerationStructureEXT(push.uses.tlas),
            ray_flags, cull_mask, sbt_record_offset, sbt_record_stride, miss_index,
            ray_pos, t_min, ray_dir, t_max, PAYLOAD_LOC);

        if (prd == miss_ray_payload()) {
            hit = 1;
        }
    }

    imageStore(daxa_image2D(push.uses.shadow_mask), px, vec4(hit, 0, 0, 0));
}

#endif
