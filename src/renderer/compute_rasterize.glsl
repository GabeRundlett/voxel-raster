#include <shared.inl>
#include <renderer/visbuffer.glsl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/meshlet_allocator.glsl>
#include <renderer/culling.glsl>

DAXA_DECL_PUSH_CONSTANT(ComputeRasterizePush, push)

DAXA_DECL_IMAGE_ACCESSOR_WITH_FORMAT(u64image2D, r64ui, , r64uiImage)

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

#define u_visbuffer daxa_access(r64uiImage, push.uses.visbuffer64)
#define SUBPIXEL_BITS 8
#define SUBPIXEL_SAMPLES (1 << SUBPIXEL_BITS)

DAXA_STORAGE_IMAGE_LAYOUT_WITH_FORMAT(r32ui)
uniform uimage2D atomic_u32_table[];

void write_pixel(ivec2 p, uint64_t payload, float depth) {
    imageAtomicMax(daxa_access(r64uiImage, push.uses.visbuffer64), p, payload | (uint64_t(floatBitsToUint(depth)) << 32));
#if ENABLE_DEBUG_VIS
    imageAtomicAdd(atomic_u32_table[daxa_image_view_id_to_index(push.uses.debug_overdraw)], p, 1);
#endif
}

void rasterize_triangle(in vec3[3] triangle, ivec2 viewport_size, PackedVisbufferPayload visbuffer_payload) {
    uint64_t payload = uint64_t(visbuffer_payload.data);
    const vec3 v01 = triangle[1] - triangle[0];
    const vec3 v02 = triangle[2] - triangle[0];
    const float det_xy = v01.x * v02.y - v01.y * v02.x;
    if (det_xy >= 0.0) {
        return;
    }

    const float inv_det = 1.0 / det_xy;
    vec2 grad_z = vec2(
        (v01.z * v02.y - v01.y * v02.z) * inv_det,
        (v01.x * v02.z - v01.z * v02.x) * inv_det);

    vec2 vert_0 = triangle[0].xy;
    vec2 vert_1 = triangle[1].xy;
    vec2 vert_2 = triangle[2].xy;

    const vec2 min_subpixel = min(min(vert_0, vert_1), vert_2);
    const vec2 max_subpixel = max(max(vert_0, vert_1), vert_2);

    ivec2 min_pixel = ivec2(floor((min_subpixel + (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / float(SUBPIXEL_SAMPLES))));
    ivec2 max_pixel = ivec2(floor((max_subpixel - (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / float(SUBPIXEL_SAMPLES))));

    min_pixel = max(min_pixel, ivec2(0));
    max_pixel = min(max_pixel, viewport_size.xy - 1);
    if (any(greaterThan(min_pixel, max_pixel))) {
        return;
    }

    max_pixel = min(max_pixel, min_pixel + 63);

    const vec2 edge_01 = -v01.xy;
    const vec2 edge_12 = vert_1 - vert_2;
    const vec2 edge_20 = v02.xy;

    const vec2 base_subpixel = vec2(min_pixel) * SUBPIXEL_SAMPLES + (SUBPIXEL_SAMPLES / 2);
    vert_0 -= base_subpixel;
    vert_1 -= base_subpixel;
    vert_2 -= base_subpixel;

    float hec_0 = edge_01.y * vert_0.x - edge_01.x * vert_0.y;
    float hec_1 = edge_12.y * vert_1.x - edge_12.x * vert_1.y;
    float hec_2 = edge_20.y * vert_2.x - edge_20.x * vert_2.y;

    hec_0 -= saturate(edge_01.y + saturate(1.0 - edge_01.x));
    hec_1 -= saturate(edge_12.y + saturate(1.0 - edge_12.x));
    hec_2 -= saturate(edge_20.y + saturate(1.0 - edge_20.x));

    const float z_0 = triangle[0].z - (grad_z.x * vert_0.x + grad_z.y * vert_0.y);
    grad_z *= SUBPIXEL_SAMPLES;

    float hec_y_0 = hec_0 * (1.0 / float(SUBPIXEL_SAMPLES));
    float hec_y_1 = hec_1 * (1.0 / float(SUBPIXEL_SAMPLES));
    float hec_y_2 = hec_2 * (1.0 / float(SUBPIXEL_SAMPLES));
    float z_y = z_0;

    if (subgroupAny(max_pixel.x - min_pixel.x > 4)) {
        const vec3 edge_012 = vec3(edge_01.y, edge_12.y, edge_20.y);
        const bvec3 is_open_edge = lessThan(edge_012, vec3(0.0));
        const vec3 inv_edge_012 = vec3(
            edge_012.x == 0 ? 1e8 : (1.0 / edge_012.x),
            edge_012.y == 0 ? 1e8 : (1.0 / edge_012.y),
            edge_012.z == 0 ? 1e8 : (1.0 / edge_012.z));
        int y = min_pixel.y;
        while (true) {
            const vec3 cross_x = vec3(hec_y_0, hec_y_1, hec_y_2) * inv_edge_012;
            const vec3 min_x = vec3(
                is_open_edge.x ? cross_x.x : 0.0,
                is_open_edge.y ? cross_x.y : 0.0,
                is_open_edge.z ? cross_x.z : 0.0);
            const vec3 max_x = vec3(
                is_open_edge.x ? max_pixel.x - min_pixel.x : cross_x.x,
                is_open_edge.y ? max_pixel.x - min_pixel.x : cross_x.y,
                is_open_edge.z ? max_pixel.x - min_pixel.x : cross_x.z);
            float x_0 = ceil(max(max(min_x.x, min_x.y), min_x.z));
            float x_1 = min(min(max_x.x, max_x.y), max_x.z);
            float z_x = z_y + grad_z.x * x_0;

            x_0 += min_pixel.x;
            x_1 += min_pixel.x;
            for (float x = x_0; x <= x_1; ++x) {
                write_pixel(ivec2(x, y), payload, z_x);
                z_x += grad_z.x;
            }

            if (y >= max_pixel.y) {
                break;
            }
            hec_y_0 += edge_01.x;
            hec_y_1 += edge_12.x;
            hec_y_2 += edge_20.x;
            z_y += grad_z.y;
            ++y;
        }
    } else {
        int y = min_pixel.y;
        while (true) {
            int x = min_pixel.x;
            if (min(min(hec_y_0, hec_y_1), hec_y_2) >= 0.0) {
                write_pixel(ivec2(x, y), payload, z_y);
            }

            if (x < max_pixel.x) {
                float hec_x_0 = hec_y_0 - edge_01.y;
                float hec_x_1 = hec_y_1 - edge_12.y;
                float hec_x_2 = hec_y_2 - edge_20.y;
                float z_x = z_y + grad_z.x;
                ++x;

                while (true) {
                    if (min(min(hec_x_0, hec_x_1), hec_x_2) >= 0.0) {
                        write_pixel(ivec2(x, y), payload, z_x);
                    }

                    if (x >= max_pixel.x) {
                        break;
                    }

                    hec_x_0 -= edge_01.y;
                    hec_x_1 -= edge_12.y;
                    hec_x_2 -= edge_20.y;
                    z_x += grad_z.x;
                    ++x;
                }
            }

            if (y >= max_pixel.y) {
                break;
            }

            hec_y_0 += edge_01.x;
            hec_y_1 += edge_12.x;
            hec_y_2 += edge_20.x;
            z_y += grad_z.y;
            ++y;
        }
    }
}

#define WORKGROUP_SIZE 128
#define MESHLETS_PER_WORKGROUP (WORKGROUP_SIZE / MAX_FACES_PER_MESHLET)

struct MeshletInfo {
    uint face_count;
};

shared BrickInstance meshlet_brick_instances[MESHLETS_PER_WORKGROUP];
shared VoxelChunk voxel_chunks[MESHLETS_PER_WORKGROUP];
shared MeshletInfo meshlet_infos[MESHLETS_PER_WORKGROUP];
shared VoxelMeshlet meshlets[MESHLETS_PER_WORKGROUP];

layout(local_size_x = WORKGROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
void main() {
    const uint meshlet_base_id = gl_WorkGroupID.x * MESHLETS_PER_WORKGROUP + 1 + deref(push.uses.indirect_info[1]).offset;
    const uint in_workgroup_meshlet_index = gl_LocalInvocationID.x / MAX_FACES_PER_MESHLET;
    const uint in_meshlet_face_index = gl_LocalInvocationID.x % MAX_FACES_PER_MESHLET;
    const uint meshlet_index = meshlet_base_id + in_workgroup_meshlet_index;
    const bool is_valid_meshlet_id = is_valid_meshlet_index(daxa_BufferPtr(VoxelMeshlet)(push.uses.meshlet_allocator), meshlet_index);

    if (is_valid_meshlet_id) {
        if (in_meshlet_face_index == 0) {
            VoxelMeshletMetadata metadata = deref(push.uses.meshlet_metadata[meshlet_index]);
            bool is_valid_brick_instance = is_valid_index(daxa_BufferPtr(BrickInstance)(push.uses.brick_instance_allocator), metadata.brick_instance_index);
            if (is_valid_brick_instance) {
                BrickInstance brick_instance = deref(push.uses.brick_instance_allocator[metadata.brick_instance_index]);
                VoxelChunk voxel_chunk = deref(push.uses.chunks[brick_instance.chunk_index]);
                VoxelBrickMesh mesh = deref(voxel_chunk.meshes[brick_instance.brick_index]);

                meshlet_brick_instances[in_workgroup_meshlet_index] = brick_instance;
                voxel_chunks[in_workgroup_meshlet_index] = voxel_chunk;
                meshlet_infos[in_workgroup_meshlet_index].face_count = mesh.face_count - (meshlet_index - mesh.meshlet_start) * 32;
            }
        }

        PackedVoxelBrickFace packed_face = deref(push.uses.meshlet_allocator[meshlet_index]).faces[in_meshlet_face_index];
        meshlets[in_workgroup_meshlet_index].faces[in_meshlet_face_index] = packed_face;
    } else {
        if (in_meshlet_face_index == 0) {
            meshlet_brick_instances[in_workgroup_meshlet_index] = BrickInstance(0, 0);
            // voxel_chunks[in_workgroup_meshlet_index] = VoxelChunk(0);
            meshlet_infos[in_workgroup_meshlet_index].face_count = 0;
        }
    }

    barrier();

    if (in_meshlet_face_index < meshlet_infos[in_workgroup_meshlet_index].face_count) {
        BrickInstance brick_instance = meshlet_brick_instances[in_workgroup_meshlet_index];
        VoxelChunk voxel_chunk = voxel_chunks[in_workgroup_meshlet_index];

        PackedVoxelBrickFace packed_face = meshlets[in_workgroup_meshlet_index].faces[in_meshlet_face_index];

        VoxelBrickFace face = unpack(packed_face);

        VisbufferPayload o_payload;
        o_payload.face_id = in_meshlet_face_index;
        o_payload.meshlet_id = meshlet_index;
        PackedVisbufferPayload o_packed_payload = pack(o_payload);

        ivec4 pos_scl = deref(voxel_chunk.pos_scl[brick_instance.brick_index]);
        ivec3 pos = ivec3(voxel_chunk.pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(face.pos);
        int scl = pos_scl.w + 8;

        // guaranteed 0, 1, or 2.
        uint axis = face.axis / 2;

        ivec3 offset = ivec3(0);
        offset[axis] = 1;
        pos += offset * int(face.axis % 2);

        int flip = int(face.axis % 2);
        // For some reason we need to flip for the y-axis
        flip = flip ^ int(axis == 1);

#define SCL (float(1 << scl) / float(1 << 8))

        int winding_flip_a = flip;
        int winding_flip_b = 1 - winding_flip_a;
        ivec3 ip0 = pos;
        ivec3 ip1 = pos + ivec3(int(axis != 0) * winding_flip_a, int(axis == 0) * winding_flip_a + int(axis == 2) * winding_flip_b, int(axis != 2) * winding_flip_b);
        ivec3 ip2 = pos + ivec3(int(axis != 0) * winding_flip_b, int(axis == 0) * winding_flip_b + int(axis == 2) * winding_flip_a, int(axis != 2) * winding_flip_a);
        ivec3 ip3 = pos + ivec3(int(axis != 0), int(axis != 1), int(axis != 2));

        mat4 world_to_clip = deref(push.uses.gpu_input).cam.view_to_clip * deref(push.uses.gpu_input).cam.world_to_view;

        ivec2 viewport_size = ivec2(deref(push.uses.gpu_input).render_size);

        const vec2 scale = vec2(0.5, 0.5) * vec2(viewport_size) * float(SUBPIXEL_SAMPLES);
        const vec2 bias = (0.5 * vec2(viewport_size)) * float(SUBPIXEL_SAMPLES) + 0.5;

        vec4 p0_h = world_to_clip * vec4(ip0 * SCL, 1);
        vec4 p1_h = world_to_clip * vec4(ip1 * SCL, 1);
        vec4 p2_h = world_to_clip * vec4(ip2 * SCL, 1);
        vec4 p3_h = world_to_clip * vec4(ip3 * SCL, 1);

        vec3 p0 = p0_h.xyz / p0_h.w;
        vec3 p1 = p1_h.xyz / p1_h.w;
        vec3 p2 = p2_h.xyz / p2_h.w;
        vec3 p3 = p3_h.xyz / p3_h.w;
        vec3 ndc_min = min(min(p0, p1), min(p2, p3));
        vec3 ndc_max = max(max(p0, p1), max(p2, p3));

#if DO_DEPTH_CULL
        if (is_ndc_aabb_hiz_depth_occluded(ndc_min, ndc_max, deref(push.uses.gpu_input).next_lower_po2_render_size, push.uses.hiz)) {
            return;
        }
#endif

#if DRAW_FROM_OBSERVER
        world_to_clip = deref(push.uses.gpu_input).observer_cam.view_to_clip * deref(push.uses.gpu_input).observer_cam.world_to_view;
        p0_h = world_to_clip * vec4(ip0 * SCL, 1);
        p1_h = world_to_clip * vec4(ip1 * SCL, 1);
        p2_h = world_to_clip * vec4(ip2 * SCL, 1);
        p3_h = world_to_clip * vec4(ip3 * SCL, 1);
        p0 = p0_h.xyz / p0_h.w;
        p1 = p1_h.xyz / p1_h.w;
        p2 = p2_h.xyz / p2_h.w;
        p3 = p3_h.xyz / p3_h.w;
#endif

        p0.xy = floor(p0.xy * scale + bias);
        p1.xy = floor(p1.xy * scale + bias);
        p2.xy = floor(p2.xy * scale + bias);
        p3.xy = floor(p3.xy * scale + bias);

        rasterize_triangle(vec3[](p0, p2, p1), viewport_size, o_packed_payload);
        rasterize_triangle(vec3[](p1, p2, p3), viewport_size, o_packed_payload);
    }
}
