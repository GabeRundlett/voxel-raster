#pragma once

#include <voxels/voxel_mesh.inl>

#if defined(__cplusplus)
#include <glm/glm.hpp>
using namespace glm;
#endif

float msign(float v) {
    return (v >= 0.0f) ? 1.0f : -1.0f;
}

vec2 map_octahedral(vec3 nor) {
    const float fac = 1.0f / (abs(nor.x) + abs(nor.y) + abs(nor.z));
    nor.x *= fac;
    nor.y *= fac;
    if (nor.z < 0.0f) {
        const vec2 temp = vec2(nor);
        nor.x = (1.0f - abs(temp.y)) * msign(temp.x);
        nor.y = (1.0f - abs(temp.x)) * msign(temp.y);
    }
    return vec2(nor.x, nor.y);
}
vec3 unmap_octahedral(vec2 v) {
    vec3 nor = vec3(v, 1.0f - abs(v.x) - abs(v.y)); // Rune Stubbe's version,
    float t = max(-nor.z, 0.0f);                    // much faster than original
    nor.x += (nor.x > 0.0f) ? -t : t;               // implementation of this
    nor.y += (nor.y > 0.0f) ? -t : t;               // technique
    return normalize(nor);
}

#define SNORM_SCALE(N) (float((1 << (N - 1u))) - 0.5f)
#define PACK_SNORM_X2(v, N)                                      \
    uvec2 d = uvec2(round(SNORM_SCALE(N) + v * SNORM_SCALE(N))); \
    return d.x | (d.y << N)
#define UNPACK_SNORM_X2(d, N) return vec2(uvec2((d), (d) >> N) & ((1u << N) - 1u)) / SNORM_SCALE(N) - 1.0f

#define UNORM_SCALE(N) (float(1 << (N)) - 1.0f)
#define PACK_UNORM(x, N) uint(round((x) * UNORM_SCALE((N))))
#define UNPACK_UNORM(x, N) (float((x) & ((1u << (N)) - 1u)) / UNORM_SCALE((N)))

uint pack_snorm_2x04(vec2 v) { PACK_SNORM_X2(v, 4); }
uint pack_snorm_2x08(vec2 v) { PACK_SNORM_X2(v, 8); }
uint pack_snorm_2x12(vec2 v) { PACK_SNORM_X2(v, 12); }
uint pack_snorm_2x16(vec2 v) { PACK_SNORM_X2(v, 16); }
vec2 unpack_snorm_2x04(uint d) { UNPACK_SNORM_X2(d, 4); }
vec2 unpack_snorm_2x08(uint d) { UNPACK_SNORM_X2(d, 8); }
vec2 unpack_snorm_2x12(uint d) { UNPACK_SNORM_X2(d, 12); }
vec2 unpack_snorm_2x16(uint d) { UNPACK_SNORM_X2(d, 16); }

uint pack_octahedral_08(vec3 nor) { return pack_snorm_2x04(map_octahedral(nor)); }
uint pack_octahedral_16(vec3 nor) { return pack_snorm_2x08(map_octahedral(nor)); }
uint pack_octahedral_24(vec3 nor) { return pack_snorm_2x12(map_octahedral(nor)); }
uint pack_octahedral_32(vec3 nor) { return pack_snorm_2x16(map_octahedral(nor)); }
vec3 unpack_octahedral_08(uint data) { return unmap_octahedral(unpack_snorm_2x04(data)); }
vec3 unpack_octahedral_16(uint data) { return unmap_octahedral(unpack_snorm_2x08(data)); }
vec3 unpack_octahedral_24(uint data) { return unmap_octahedral(unpack_snorm_2x12(data)); }
vec3 unpack_octahedral_32(uint data) { return unmap_octahedral(unpack_snorm_2x16(data)); }

uint pack_rgb565(vec3 col) { return (PACK_UNORM(col.r, 5) << 0) | (PACK_UNORM(col.g, 6) << 5) | (PACK_UNORM(col.b, 5) << 11); }
vec3 unpack_rgb565(uint data) { return vec3(UNPACK_UNORM(data >> 0, 5), UNPACK_UNORM(data >> 5, 6), UNPACK_UNORM(data >> 11, 5)); }

PackedVoxel pack_voxel(Voxel v) { return PackedVoxel(pack_rgb565(vec3(v.col.x, v.col.y, v.col.z)) | (pack_octahedral_16(vec3(v.nrm.x, v.nrm.y, v.nrm.z)) << 16)); }
Voxel unpack_voxel(PackedVoxel v) {
    vec3 col = unpack_rgb565(v.data >> 0);
    vec3 nrm = unpack_octahedral_16(v.data >> 16);
    return Voxel(daxa_f32vec3(col.x, col.y, col.z), daxa_f32vec3(nrm.x, nrm.y, nrm.z));
}

#undef SNORM_SCALE
#undef PACK_SNORM_X2
#undef UNPACK_SNORM_X2
#undef UNORM_SCALE
#undef PACK_UNORM
#undef UNPACK_UNORM
