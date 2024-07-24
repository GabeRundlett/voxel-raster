#pragma once

#define PAYLOAD_LOC 0

struct Ray {
    vec3 origin;
    vec3 direction;
};

#if defined(VOXEL_RT_ANY_HIT)
struct RayPayload {
    bool hit;
};
struct PackedRayPayload {
    bool hit;
};

PackedRayPayload pack_ray_payload(bool hit) {
    return PackedRayPayload(hit);
}

PackedRayPayload miss_ray_payload() {
    return pack_ray_payload(false);
}

RayPayload unpack_ray_payload(PackedRayPayload payload) {
    RayPayload result;
    result.hit = payload.hit;
    return result;
}
#else
struct RayPayload {
    uint chunk_id;
    uint brick_id;
    ivec3 voxel_i;
};

struct PackedRayPayload {
    uint data0;
    uint data1;
};

struct PackedHitAttribute {
    uint data;
};

ivec3 unpack_hit_attribute(PackedHitAttribute hit_attrib) {
    ivec3 result;
    result.x = int(hit_attrib.data % VOXEL_BRICK_SIZE);
    result.y = int(hit_attrib.data / VOXEL_BRICK_SIZE % VOXEL_BRICK_SIZE);
    result.z = int(hit_attrib.data / VOXEL_BRICK_SIZE / VOXEL_BRICK_SIZE);
    return result;
}

PackedHitAttribute pack_hit_attribute(ivec3 voxel_i) {
    return PackedHitAttribute(voxel_i.x + voxel_i.y * VOXEL_BRICK_SIZE + voxel_i.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE);
}

PackedRayPayload pack_ray_payload(uint blas_id, uint brick_id, PackedHitAttribute hit_attrib) {
    return PackedRayPayload(blas_id, (brick_id * (VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * 2)) | hit_attrib.data);
}

PackedRayPayload miss_ray_payload() {
    return pack_ray_payload(0, 0, PackedHitAttribute(VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE));
}

RayPayload unpack_ray_payload(PackedRayPayload payload) {
    RayPayload result;
    result.chunk_id = payload.data0;
    result.brick_id = payload.data1 / (VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * 2);
    result.voxel_i = unpack_hit_attribute(PackedHitAttribute(payload.data1 % (VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * 2)));
    return result;
}
#endif

float hitAabb(const Aabb aabb, const Ray r, bool guaranteed) {
    if (all(greaterThanEqual(r.origin, aabb.minimum)) && all(lessThanEqual(r.origin, aabb.maximum))) {
        return 0.0;
    }
    vec3 invDir = 1.0 / r.direction;
    vec3 tbot = invDir * (aabb.minimum - r.origin);
    vec3 ttop = invDir * (aabb.maximum - r.origin);
    vec3 tmin = min(ttop, tbot);
    vec3 tmax = max(ttop, tbot);
    float t0 = max(tmin.x, max(tmin.y, tmin.z));
    float t1 = min(tmax.x, min(tmax.y, tmax.z));
    if (guaranteed) {
        return t0;
    } else {
        return t1 >= max(t0, 0.0) ? t0 : -1.0;
    }
}

vec3 hit_aabb_nrm(const Aabb aabb, const Ray r, bool guaranteed) {
    if (all(greaterThanEqual(r.origin, aabb.minimum)) && all(lessThanEqual(r.origin, aabb.maximum))) {
        return vec3(0.0);
    }
    vec3 invDir = 1.0 / r.direction;
    vec3 tbot = invDir * (aabb.minimum - r.origin);
    vec3 ttop = invDir * (aabb.maximum - r.origin);
    vec3 tmin = min(ttop, tbot);
    vec3 tmax = max(ttop, tbot);
    float t0 = max(tmin.x, max(tmin.y, tmin.z));
    float t1 = min(tmax.x, min(tmax.y, tmax.z));

    if (!guaranteed && t1 < max(t0, 0.0)) {
        return vec3(-1);
    }

    if (t0 == tmin.x) {
        return vec3(-sign(r.direction.x), 0, 0);
    } else if (t0 == tmin.y) {
        return vec3(0, -sign(r.direction.y), 0);
    } else {
        return vec3(0, 0, -sign(r.direction.z));
    }
}

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_INTERSECTION || DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_CLOSEST_HIT
#if !defined(VOXEL_RT_ANY_HIT)
hitAttributeEXT PackedHitAttribute hit_attrib;
#endif
#else
#extension GL_EXT_ray_query : enable
rayQueryEXT ray_query;
#if !defined(VOXEL_RT_ANY_HIT)
PackedHitAttribute hit_attrib;
#endif
#endif

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_INTERSECTION || DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

#include <voxels/voxel_mesh.glsl>

void intersect_voxel_brick() {
    Ray ray;

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_INTERSECTION
#define OBJECT_TO_WORLD_MAT gl_ObjectToWorld3x4EXT
#define INSTANCE_CUSTOM_INDEX gl_InstanceCustomIndexEXT
#define PRIMITIVE_INDEX gl_PrimitiveID
    ray.origin = gl_ObjectRayOriginEXT;
    ray.direction = gl_ObjectRayDirectionEXT;
#else
    const mat3x4 object_to_world_mat = transpose(rayQueryGetIntersectionObjectToWorldEXT(ray_query, false));
#define OBJECT_TO_WORLD_MAT object_to_world_mat
#define INSTANCE_CUSTOM_INDEX rayQueryGetIntersectionInstanceCustomIndexEXT(ray_query, false)
#define PRIMITIVE_INDEX rayQueryGetIntersectionPrimitiveIndexEXT(ray_query, false)
    ray.origin = rayQueryGetIntersectionObjectRayOriginEXT(ray_query, false);
    ray.direction = rayQueryGetIntersectionObjectRayDirectionEXT(ray_query, false);
#endif

    ray.origin = (OBJECT_TO_WORLD_MAT * ray.origin).xyz;
    ray.direction = (OBJECT_TO_WORLD_MAT * ray.direction).xyz;
    float tHit = -1;
    daxa_BufferPtr(VoxelChunk) voxel_chunk = advance(push.uses.chunks, INSTANCE_CUSTOM_INDEX);
    Aabb aabb = deref(advance(deref(voxel_chunk).aabbs, PRIMITIVE_INDEX));
    tHit = hitAabb(aabb, ray, false);
    const float BIAS = uintBitsToFloat(0x3f800040); // uintBitsToFloat(0x3f800040) == 1.00000762939453125
    ray.origin += ray.direction * tHit * BIAS;
    if (tHit >= 0) {
        ivec3 bmin = ivec3(floor(aabb.minimum * VOXEL_SCL));
        ivec3 mapPos = clamp(ivec3(floor(ray.origin * VOXEL_SCL)) - bmin, ivec3(0), ivec3(VOXEL_BRICK_SIZE - 1));
        vec3 deltaDist = abs(vec3(length(ray.direction)) / ray.direction);
        vec3 sideDist = (sign(ray.direction) * (vec3(mapPos + bmin) - ray.origin * VOXEL_SCL) + (sign(ray.direction) * 0.5) + 0.5) * deltaDist;
        ivec3 rayStep = ivec3(sign(ray.direction));
        bvec3 mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
        for (int i = 0; i < int(3 * VOXEL_BRICK_SIZE); i++) {
            if (load_bit(daxa_BufferPtr(GpuInput)(push.uses.gpu_input),
                         daxa_BufferPtr(VoxelBrickBitmask)(deref(voxel_chunk).bitmasks[PRIMITIVE_INDEX]),
                         daxa_BufferPtr(ivec4)(deref(voxel_chunk).pos_scl[PRIMITIVE_INDEX]),
                         uint(mapPos.x), uint(mapPos.y), uint(mapPos.z)) == 1) {
#if defined(VOXEL_RT_ANY_HIT)
                // NOTE: tHit is "wrong" here, but we don't care because this is a specialization
                // to help potentially save registers for raycasts that don't care about the dist
                // and instead just want to know if there is any hit at all (such as shadows)
#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_INTERSECTION
                reportIntersectionEXT(1, 0);
#else
                rayQueryGenerateIntersectionEXT(ray_query, 1);
#endif
#else
                aabb.minimum += vec3(mapPos) * VOXEL_SIZE;
                aabb.maximum = aabb.minimum + VOXEL_SIZE;
                tHit += hitAabb(aabb, ray, true);
#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_INTERSECTION
                hit_attrib = pack_hit_attribute(mapPos);
                reportIntersectionEXT(tHit, 0);
#else
                if (tHit < rayQueryGetIntersectionTEXT(ray_query, true)) {
                    hit_attrib = pack_hit_attribute(mapPos);
                    rayQueryGenerateIntersectionEXT(ray_query, tHit);
                }
#endif
#endif
                break;
            }
            mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
            sideDist += vec3(mask) * deltaDist;
            mapPos += ivec3(vec3(mask)) * rayStep;
            bool outside_l = any(lessThan(mapPos, ivec3(0)));
            bool outside_g = any(greaterThanEqual(mapPos, ivec3(VOXEL_BRICK_SIZE)));
            if ((int(outside_l) | int(outside_g)) != 0) {
                break;
            }
        }
    }
}
#endif

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_INTERSECTION
void main() {
    intersect_voxel_brick();
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_CLOSEST_HIT
layout(location = PAYLOAD_LOC) rayPayloadInEXT PackedRayPayload prd;
void main() {
#if defined(VOXEL_RT_ANY_HIT)
    prd = pack_ray_payload(true);
#else
    prd = pack_ray_payload(gl_InstanceCustomIndexEXT, gl_PrimitiveID, hit_attrib);
#endif
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_MISS
layout(location = PAYLOAD_LOC) rayPayloadInEXT PackedRayPayload prd;
void main() {
    prd = miss_ray_payload();
}
#endif
