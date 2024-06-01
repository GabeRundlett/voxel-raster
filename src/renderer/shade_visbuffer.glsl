#include <shared.inl>
#include <renderer/visbuffer.glsl>
#include <voxels/pack_unpack.inl>
#include <voxels/voxel_mesh.glsl>

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

const vec3 SKY_COL = vec3(100, 121, 255) / 255;

void main() {
#if VISUALIZE_OVERDRAW
    uint overdraw = texelFetch(daxa_utexture2D(push.uses.debug_overdraw), ivec2(gl_FragCoord.xy), 0).x;
    const uint threshold = 10;
    f_out = vec4(color_correct(hsv2rgb(vec3(clamp(float(overdraw) / threshold, 0, 1) * 0.4 + 0.65, 0.99, overdraw > threshold ? exp(-float(overdraw - threshold) * 0.5 / threshold) * 0.8 + 0.2 : 1))), 1);
#else
    uint visbuffer_id = texelFetch(daxa_utexture2D(push.uses.visbuffer), ivec2(gl_FragCoord.xy), 0).x;
    if (visbuffer_id == INVALID_MESHLET_INDEX) {
        f_out = vec4(SKY_COL, 255);
    } else {
        VisbufferPayload payload = unpack(PackedVisbufferPayload(visbuffer_id));

        VoxelMeshlet meshlet = deref(push.uses.meshlet_allocator[payload.meshlet_id]);
        PackedVoxelBrickFace packed_face = meshlet.faces[payload.face_id];
        VoxelBrickFace face = unpack(packed_face);

        VoxelMeshletMetadata metadata = deref(push.uses.meshlet_metadata[payload.meshlet_id]);
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
#endif
}

#endif
