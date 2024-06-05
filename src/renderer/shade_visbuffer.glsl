#include <shared.inl>
#include <renderer/visbuffer.glsl>
#include <voxels/pack_unpack.inl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/meshlet_allocator.glsl>
#include <camera.glsl>

DAXA_DECL_PUSH_CONSTANT(ShadeVisbufferPush, push)

vec4 f_out;
uint visbuffer_id;
float depth;

vec3 hsv2rgb(in vec3 c) {
    // https://www.shadertoy.com/view/MsS3Wc
    vec3 rgb = clamp(abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return c.z * mix(vec3(1.0), rgb, c.y);
}
float hash11(float p) {
    p = fract(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

#define VISUALIZE_OVERDRAW (ENABLE_DEBUG_VIS && 0)
#define VISUALIZE_BRICK_SIZE 0

const vec3 SKY_COL = vec3(20, 20, 255) / 255;

void visualize_overdraw() {
    uint overdraw = texelFetch(daxa_utexture2D(push.uses.debug_overdraw), ivec2(gl_GlobalInvocationID.xy), 0).x;
    const uint threshold = 10;
    f_out = vec4(hsv2rgb(vec3(clamp(float(overdraw) / threshold, 0, 1) * 0.4 + 0.65, 0.99, overdraw > threshold ? exp(-float(overdraw - threshold) * 0.5 / threshold) * 0.8 + 0.2 : 1)), 1);
}

void visualize_primitive_size() {
    if (visbuffer_id == INVALID_MESHLET_INDEX) {
        f_out = vec4(SKY_COL, 1);
    } else {
        VisbufferPayload payload = unpack(PackedVisbufferPayload(visbuffer_id));
        VoxelMeshlet meshlet = deref(push.uses.meshlet_allocator[payload.meshlet_id]);
        PackedVoxelBrickFace packed_face = meshlet.faces[payload.face_id];
        VoxelBrickFace face = unpack(packed_face);

        VoxelMeshletMetadata metadata = deref(push.uses.meshlet_metadata[payload.meshlet_id]);
        if (!is_valid_index(daxa_BufferPtr(BrickInstance)(push.uses.brick_instance_allocator), metadata.brick_instance_index)) {
            return;
        }

        BrickInstance brick_instance = deref(push.uses.brick_instance_allocator[metadata.brick_instance_index]);

        VoxelChunk voxel_chunk = deref(push.uses.chunks[brick_instance.chunk_index]);
        uint voxel_index = face.pos.x + face.pos.y * VOXEL_BRICK_SIZE + face.pos.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
        Voxel voxel = unpack_voxel(deref(voxel_chunk.attribs[brick_instance.brick_index]).packed_voxels[voxel_index]);

        ivec4 pos_scl = deref(voxel_chunk.pos_scl[brick_instance.brick_index]);
        ivec3 pos = ivec3(voxel_chunk.pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(face.pos);
        int scl = pos_scl.w + 8;
        const float SCL = (float(1 << scl) / float(1 << 8));

        vec3 ndc_min;
        vec3 ndc_max;

        float size_x = brick_extent_pixels(push.uses.gpu_input, push.uses.chunks, brick_instance).x / VOXEL_BRICK_SIZE;

        float size = size_x;

        const float threshold = 32;

        // vec3 albedo = hsv2rgb(vec3(clamp(float(size) / threshold, 0, 1) * 0.4 + 0.65, 0.99, size > threshold ? exp(-float(size - threshold) * 0.5 / threshold) * 0.8 + 0.2 : 1));
        vec3 albedo = size < threshold ? (size < 4 ? vec3(0.4, 0.4, 0.9) : vec3(0.2, 0.9, 0.2)) : vec3(0.9, 0.2, 0.2);

        vec3 diffuse = vec3(0);
        // diffuse += vec3(1);
        diffuse += max(0.0, dot(voxel.nrm, normalize(vec3(-1, 2, -3)))) * vec3(0.9, 0.7, 0.5) * 2;
        diffuse += max(0.0, dot(voxel.nrm, normalize(vec3(0, 0, -1))) * 0.4 + 0.6) * SKY_COL;

        f_out = vec4(albedo * diffuse, 1);
    }
}

void shade() {
    if (visbuffer_id == INVALID_MESHLET_INDEX) {
        f_out = vec4(SKY_COL, 1);
    } else {
        VisbufferPayload payload = unpack(PackedVisbufferPayload(visbuffer_id));

        VoxelMeshlet meshlet = deref(push.uses.meshlet_allocator[payload.meshlet_id]);
        PackedVoxelBrickFace packed_face = meshlet.faces[payload.face_id];
        VoxelBrickFace face = unpack(packed_face);

        VoxelMeshletMetadata metadata = deref(push.uses.meshlet_metadata[payload.meshlet_id]);
        if (!is_valid_index(daxa_BufferPtr(BrickInstance)(push.uses.brick_instance_allocator), metadata.brick_instance_index)) {
            return;
        }

        BrickInstance brick_instance = deref(push.uses.brick_instance_allocator[metadata.brick_instance_index]);

        VoxelChunk voxel_chunk = deref(push.uses.chunks[brick_instance.chunk_index]);
        uint voxel_index = face.pos.x + face.pos.y * VOXEL_BRICK_SIZE + face.pos.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
        Voxel voxel = unpack_voxel(deref(voxel_chunk.attribs[brick_instance.brick_index]).packed_voxels[voxel_index]);

        // vec3 face_nrm = vec3(0, 0, 0);
        // switch (face.axis / 2) {
        // case 0: face_nrm.x = float(face.axis % 2) * 2.0 - 1.0; break;
        // case 1: face_nrm.y = float(face.axis % 2) * 2.0 - 1.0; break;
        // case 2: face_nrm.z = float(face.axis % 2) * 2.0 - 1.0; break;
        // }
        // voxel.nrm = normalize(face_nrm * 0.25 + voxel.nrm);

        const vec3 voxel_center = (vec3(face.pos) + 0.5);

        vec3 albedo = vec3(1);
        // albedo = vec3(voxel.nrm);
        // albedo = voxel_center / VOXEL_BRICK_SIZE;
        // albedo = hsv2rgb(vec3(hash11(payload.meshlet_id), hash11(payload.face_id) * 0.4 + 0.7, 0.9));
        // albedo = hsv2rgb(vec3(hash11(brick_instance.chunk_index) * 0.8 + hash11(brick_instance.brick_index) * 0.2, hash11(packed_face.data) * 0.4 + 0.6, 0.9));
        albedo = voxel.col;

        vec3 diffuse = vec3(0);
        // diffuse += vec3(1);
        diffuse += max(0.0, dot(voxel.nrm, normalize(vec3(-1, 2, -3)))) * vec3(0.9, 0.7, 0.5) * 2;
        diffuse += max(0.0, dot(voxel.nrm, normalize(vec3(0, 0, -1))) * 0.4 + 0.6) * SKY_COL;

        vec3 out_col = albedo * diffuse;
        // if (visbuffer_id64 == INVALID_MESHLET_INDEX) {
        //     out_col = mix(out_col, vec3(1, 0, 1), 1.0);
        // } else {
        //     out_col = mix(out_col, SKY_COL, 0.9);
        // }

        f_out = vec4(out_col, 1);
    }
}

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
    uvec2 px = gl_GlobalInvocationID.xy;
    if (any(greaterThan(px, deref(push.uses.gpu_input).render_size))) {
        return;
    }

    // uint visbuffer_id = texelFetch(daxa_utexture2D(push.uses.visbuffer), ivec2(px), 0).x;
    uint64_t visbuffer64_val = imageLoad(daxa_u64image2D(push.uses.visbuffer64), ivec2(px)).x;
    visbuffer_id = uint(visbuffer64_val);
    depth = uintBitsToFloat(uint(visbuffer64_val >> uint64_t(32)));

    vec4 output_tex_size = vec4(deref(push.uses.gpu_input).render_size, 0, 0);
    output_tex_size.zw = 1.0 / output_tex_size.xy;
    vec2 uv = get_uv(px, output_tex_size);

    vec2 uv_diff;

    if (depth == 0) {
        vec4 pos_cs = vec4(uv_to_cs(uv), 0.0, 1.0);
        vec4 pos_vs = deref(push.uses.gpu_input).cam.clip_to_view * pos_cs;

        vec4 prev_vs = pos_vs;

        vec4 prev_cs = deref(push.uses.gpu_input).cam.view_to_clip * prev_vs;
        vec4 prev_pcs = deref(push.uses.gpu_input).cam.clip_to_prev_clip * prev_cs;

        vec2 prev_uv = cs_to_uv(prev_pcs.xy);
        uv_diff = prev_uv - uv;
    } else {
        ViewRayContext view_ray_context = vrc_from_uv_and_depth(push.uses.gpu_input, uv, depth);
        vec3 world_pos = ray_origin_ws(view_ray_context);

        vec3 vel_ws = vec3(0);

        vec4 vs_pos = (deref(push.uses.gpu_input).cam.world_to_view * vec4(world_pos, 1));
        vec4 prev_vs_pos = (deref(push.uses.gpu_input).cam.world_to_view * vec4(world_pos + vel_ws, 1));
        vec3 vs_velocity = (prev_vs_pos.xyz / prev_vs_pos.w) - (vs_pos.xyz / vs_pos.w);

        vec4 pos_cs = vec4(uv_to_cs(uv), depth, 1.0);
        vec4 pos_vs = (deref(push.uses.gpu_input).cam.clip_to_view * pos_cs);
        float dist_to_point = -(pos_vs.z / pos_vs.w);

        vec4 prev_vs = pos_vs / pos_vs.w;
        prev_vs.xyz += vec4(vs_velocity, 0).xyz;

        vec4 prev_cs = (deref(push.uses.gpu_input).cam.view_to_clip * prev_vs);
        vec4 prev_pcs = (deref(push.uses.gpu_input).cam.clip_to_prev_clip * prev_cs);

        vec2 prev_uv = cs_to_uv(prev_pcs.xy / prev_pcs.w);
        uv_diff = prev_uv - uv;

        // Account for quantization of the `uv_diff` in R16G16B16A16_SNORM.
        // This is so we calculate validity masks for pixels that the users will actually be using.
        uv_diff = floor(uv_diff * 32767.0 + 0.5) / 32767.0;
        prev_uv = uv + uv_diff;
    }

#if VISUALIZE_BRICK_SIZE
    visualize_primitive_size();
#elif VISUALIZE_OVERDRAW
    visualize_overdraw();
#else
    shade();
#endif

    imageStore(daxa_image2D(push.uses.color), ivec2(px), f_out);
    imageStore(daxa_image2D(push.uses.depth), ivec2(px), vec4(depth, 0, 0, 0));
    imageStore(daxa_image2D(push.uses.motion_vectors), ivec2(px), vec4(uv_diff, 0, 0));
}
