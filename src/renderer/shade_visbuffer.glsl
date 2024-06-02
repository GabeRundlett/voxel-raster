#include <shared.inl>
#include <renderer/visbuffer.glsl>
#include <voxels/pack_unpack.inl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/meshlet_allocator.glsl>

DAXA_DECL_PUSH_CONSTANT(ShadeVisbufferPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

void main() {
    vec2 uv = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1);
    gl_Position = vec4(uv * 4 - 1, 0, 1);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) out vec4 f_out;

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

const mat3 SRGB_2_XYZ_MAT = mat3(
    0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041);
const float SRGB_ALPHA = 0.055;

float luminance(vec3 color) {
    vec3 luminanceCoefficients = SRGB_2_XYZ_MAT[1];
    return dot(color, luminanceCoefficients);
}

const mat3 agxTransform = mat3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772, 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104);

const mat3 agxTransformInverse = mat3(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116);

vec3 agxDefaultContrastApproximation(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;

    return +15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

void agx(inout vec3 color) {
    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    color = agxTransform * color;
    color = clamp(log2(color), minEv, maxEv);
    color = (color - minEv) / (maxEv - minEv);
    color = agxDefaultContrastApproximation(color);
}

void agxEotf(inout vec3 color) {
    color = agxTransformInverse * color;
}

void agxLook(inout vec3 color) {
    // Punchy
    const vec3 slope = vec3(1.0);
    const vec3 power = vec3(1.0);
    const float saturation = 1.5;

    float luma = luminance(color);

    color = pow(color * slope, power);
    color = max(luma + saturation * (color - luma), vec3(0.0));
}

vec3 color_correct(vec3 x) {
    agx(x);
    agxLook(x);
    agxEotf(x);
    return x;
}

#define VISUALIZE_OVERDRAW (ENABLE_DEBUG_VIS && 0)
#define VISUALIZE_TRI_SIZE 0
#define VISUALIZE_BRICK_SIZE 0

const vec3 SKY_COL = vec3(100, 121, 255) / 255;

void visualize_overdraw() {
    uint overdraw = texelFetch(daxa_utexture2D(push.uses.debug_overdraw), ivec2(gl_FragCoord.xy), 0).x;
    const uint threshold = 10;
    f_out = vec4(color_correct(hsv2rgb(vec3(clamp(float(overdraw) / threshold, 0, 1) * 0.4 + 0.65, 0.99, overdraw > threshold ? exp(-float(overdraw - threshold) * 0.5 / threshold) * 0.8 + 0.2 : 1))), 1);
}

void visualize_primitive_size() {
    uint visbuffer_id = texelFetch(daxa_utexture2D(push.uses.visbuffer), ivec2(gl_FragCoord.xy), 0).x;
    if (visbuffer_id == INVALID_MESHLET_INDEX) {
        f_out = vec4(SKY_COL, 255);
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

        if (VISUALIZE_BRICK_SIZE != 0) {
            vec3 p0 = ivec3(voxel_chunk.pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(0);
            vec3 p1 = ivec3(voxel_chunk.pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(VOXEL_BRICK_SIZE);
            p0 *= SCL;
            p1 *= SCL;

            vec3 vertices[8] = vec3[8](
                vec3(p0.x, p0.y, p0.z),
                vec3(p1.x, p0.y, p0.z),
                vec3(p0.x, p1.y, p0.z),
                vec3(p1.x, p1.y, p0.z),
                vec3(p0.x, p0.y, p1.z),
                vec3(p1.x, p0.y, p1.z),
                vec3(p0.x, p1.y, p1.z),
                vec3(p1.x, p1.y, p1.z));

            [[unroll]] for (uint vert_i = 0; vert_i < 8; ++vert_i) {
                vec4 vs_h = deref(push.uses.gpu_input).cam.world_to_view * vec4(vertices[vert_i], 1);
                vec4 cs_h = deref(push.uses.gpu_input).cam.view_to_clip * vs_h;
                vec3 p = cs_h.xyz / cs_h.w;
                if (vert_i == 0) {
                    ndc_min = p;
                    ndc_max = p;
                } else {
                    ndc_min = min(ndc_min, p);
                    ndc_max = max(ndc_max, p);
                }
            }
            ndc_min /= VOXEL_BRICK_SIZE;
            ndc_max /= VOXEL_BRICK_SIZE;
        } else if (VISUALIZE_TRI_SIZE != 0) {
            // guaranteed 0, 1, or 2.
            uint axis = face.axis / 2;

            ivec3 offset = ivec3(0);
            offset[axis] = 1;
            pos += offset * int(face.axis % 2);

            int flip = int(face.axis % 2);
            // For some reason we need to flip for the y-axis
            flip = flip ^ int(axis == 1);

            int winding_flip_a = flip;
            int winding_flip_b = 1 - winding_flip_a;
            ivec3 in_p0 = pos;
            ivec3 in_p1 = pos + ivec3(int(axis != 0) * winding_flip_a, int(axis == 0) * winding_flip_a + int(axis == 2) * winding_flip_b, int(axis != 2) * winding_flip_b);
            ivec3 in_p2 = pos + ivec3(int(axis != 0) * winding_flip_b, int(axis == 0) * winding_flip_b + int(axis == 2) * winding_flip_a, int(axis != 2) * winding_flip_a);
            ivec3 in_p3 = pos + ivec3(int(axis != 0), int(axis != 1), int(axis != 2));

            mat4 world_to_clip = deref(push.uses.gpu_input).cam.view_to_clip * deref(push.uses.gpu_input).cam.world_to_view;

            vec4 p0_h = world_to_clip * vec4(in_p0 * SCL, 1);
            vec4 p1_h = world_to_clip * vec4(in_p1 * SCL, 1);
            vec4 p2_h = world_to_clip * vec4(in_p2 * SCL, 1);
            vec4 p3_h = world_to_clip * vec4(in_p3 * SCL, 1);

            vec3 p0 = p0_h.xyz / p0_h.w;
            vec3 p1 = p1_h.xyz / p1_h.w;
            vec3 p2 = p2_h.xyz / p2_h.w;
            vec3 p3 = p3_h.xyz / p3_h.w;

            ndc_min = min(min(p0, p1), min(p2, p3));
            ndc_max = max(max(p0, p1), max(p2, p3));
        }

        vec2 ndc_extent = ndc_max.xy - ndc_min.xy;
        vec2 pixel_extent = ndc_extent * 0.5 * deref(push.uses.gpu_input).render_size;

        float size = max(pixel_extent.x, pixel_extent.y);

        const float threshold = 32;

        // vec3 albedo = color_correct(hsv2rgb(vec3(clamp(float(size) / threshold, 0, 1) * 0.4 + 0.65, 0.99, size > threshold ? exp(-float(size - threshold) * 0.5 / threshold) * 0.8 + 0.2 : 1)));
        vec3 albedo = size < threshold ? (size < 4 ? vec3(0.4, 0.4, 0.9) : vec3(0.2, 0.9, 0.2)) : vec3(0.9, 0.2, 0.2);

        vec3 diffuse = vec3(0);
        // diffuse += vec3(1);
        diffuse += max(0.0, dot(voxel.nrm, normalize(vec3(-1, 2, -3)))) * vec3(0.9, 0.7, 0.5) * 2;
        diffuse += max(0.0, dot(voxel.nrm, normalize(vec3(0, 0, -1))) * 0.4 + 0.6) * SKY_COL;

        f_out = vec4(albedo * diffuse, 1);
    }
}

void shade() {
    uint visbuffer_id = texelFetch(daxa_utexture2D(push.uses.visbuffer), ivec2(gl_FragCoord.xy), 0).x;
    if (visbuffer_id == INVALID_MESHLET_INDEX) {
        f_out = vec4(SKY_COL, 255);
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

        f_out = vec4(color_correct(albedo * diffuse), 1);
    }
}

void main() {
#if VISUALIZE_TRI_SIZE || VISUALIZE_BRICK_SIZE
    visualize_primitive_size();
#elif VISUALIZE_OVERDRAW
    visualize_overdraw();
#else
    shade();
#endif
}

#endif
