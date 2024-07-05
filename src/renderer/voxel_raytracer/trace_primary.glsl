#define DAXA_RAY_TRACING 1
#extension GL_EXT_ray_tracing : enable

#include "voxel_raytracer.inl"
DAXA_DECL_PUSH_CONSTANT(TracePrimaryPush, push)
#include "rt.glsl"

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_RAYGEN
#include "camera.glsl"
#include <voxels/voxel_mesh.glsl>

layout(location = PAYLOAD_LOC) rayPayloadEXT PackedRayPayload prd;

const vec3 SKY_COL = vec3(20, 20, 255) / 255;
const vec3 SUN_COL = vec3(0.9, 0.7, 0.5) * 2;

void main() {
    const ivec2 px = ivec2(gl_LaunchIDEXT.xy);

    vec4 output_tex_size = vec4(deref(push.uses.gpu_input).render_size, 0, 0);
    output_tex_size.zw = vec2(1.0, 1.0) / output_tex_size.xy;
    vec2 uv = get_uv(gl_LaunchIDEXT.xy, output_tex_size);

    ViewRayContext vrc = vrc_from_uv(push.uses.gpu_input, uv);
    vec3 ray_d = ray_dir_ws(vrc);
    vec3 ray_o = ray_origin_ws(vrc);

    const uint ray_flags = gl_RayFlagsNoneEXT;
    const uint cull_mask = 0xFF;
    const uint sbt_record_offset = 0;
    const uint sbt_record_stride = 0;
    const uint miss_index = 0;
    const float t_min = 0.0001;
    const float t_max = 10000.0;

    traceRayEXT(
        accelerationStructureEXT(push.uses.tlas),
        ray_flags, cull_mask, sbt_record_offset, sbt_record_stride, miss_index,
        ray_o, t_min, ray_d, t_max, PAYLOAD_LOC);

    vec3 color = vec3(0);
    float depth = 0;
    vec2 uv_diff = vec2(0);

    RayPayload payload = unpack_ray_payload(prd);

    daxa_BufferPtr(VoxelChunk) voxel_chunk = advance(push.uses.chunks, payload.chunk_id);
    ivec4 pos_scl = deref(advance(deref(voxel_chunk).pos_scl, payload.brick_id));
    ivec3 pos = ivec3(deref(voxel_chunk).pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(payload.voxel_i);
    int scl = pos_scl.w + 8;
#define SCL (float(1 << scl) / float(1 << 8))

    Aabb aabb;
    aabb.minimum = vec3(pos) * SCL;
    aabb.maximum = aabb.minimum + SCL;

    float t_hit = hitAabb(aabb, Ray(ray_o, ray_d), true);

    if (prd.data1 == miss_ray_payload().data1) {
        vec4 pos_cs = vec4(uv_to_cs(uv), 0.0, 1.0);
        vec4 pos_vs = deref(push.uses.gpu_input).cam.clip_to_view * pos_cs;

        vec4 prev_vs = pos_vs;

        vec4 prev_cs = deref(push.uses.gpu_input).cam.view_to_clip * prev_vs;
        vec4 prev_pcs = deref(push.uses.gpu_input).cam.clip_to_prev_clip * prev_cs;

        vec2 prev_uv = cs_to_uv(prev_pcs.xy);
        uv_diff = prev_uv - uv;

        imageStore(daxa_image2D(push.uses.depth), ivec2(px), vec4(depth, 0, 0, 0));
        imageStore(daxa_image2D(push.uses.motion_vectors), ivec2(px), vec4(uv_diff, 0, 0));
        return;
    }

    vec3 hit_pos = ray_o + ray_d * t_hit;
    mat4 world_to_clip = deref(push.uses.gpu_input).cam.view_to_sample * deref(push.uses.gpu_input).cam.world_to_view;
    vec4 hit_cs_h = world_to_clip * vec4(hit_pos, 1);
    vec3 hit_cs = hit_cs_h.xyz / hit_cs_h.w;
    depth = hit_cs.z;

    {
        vec3 world_pos = hit_pos;

        vec3 vel_ws = vec3(0);

        vec4 vs_pos = (deref(push.uses.gpu_input).cam.world_to_view * vec4(world_pos, 1));
        vec4 prev_vs_pos = (deref(push.uses.gpu_input).cam.world_to_view * vec4(world_pos + vel_ws, 1));
        vec3 vs_velocity = (prev_vs_pos.xyz / prev_vs_pos.w) - (vs_pos.xyz / vs_pos.w);

        vec4 pos_cs = vec4(uv_to_cs(uv), depth, 1.0);
        vec4 pos_vs = (deref(push.uses.gpu_input).cam.clip_to_view * pos_cs);

        vec4 prev_vs = pos_vs / pos_vs.w + vec4(vs_velocity, 0);

        vec4 prev_cs = (deref(push.uses.gpu_input).cam.view_to_clip * prev_vs);
        vec4 prev_pcs = (deref(push.uses.gpu_input).cam.clip_to_prev_clip * prev_cs);

        vec2 prev_uv = cs_to_uv(prev_pcs.xy / prev_pcs.w);
        uv_diff = prev_uv - uv;

        // Account for quantization of the `uv_diff` in R16G16B16A16_SNORM.
        // This is so we calculate validity masks for pixels that the users will actually be using.
        uv_diff = floor(uv_diff * 32767.0 + 0.5) / 32767.0;
        prev_uv = uv + uv_diff;
    }

    Voxel voxel = load_voxel(push.uses.gpu_input, advance(push.uses.chunks, payload.chunk_id), payload.brick_id, payload.voxel_i);

    vec3 albedo = vec3(1);
    albedo = voxel.col;

#if !PER_VOXEL_SHADING
    voxel.nrm = hit_aabb_nrm(aabb, Ray(ray_o, ray_d), true);
#endif

    imageStore(daxa_image2D(push.uses.color), ivec2(px), vec4(albedo, 0));
    imageStore(daxa_image2D(push.uses.depth), ivec2(px), vec4(depth, 0, 0, 0));
    imageStore(daxa_image2D(push.uses.normal), ivec2(px), vec4(map_octahedral(voxel.nrm), 0, 0));
    imageStore(daxa_image2D(push.uses.motion_vectors), ivec2(px), vec4(uv_diff, 0, 0));
}

#endif
