#include <shared.inl>
#include <renderer/visbuffer.glsl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/allocators.glsl>
#include <renderer/culling.glsl>

DAXA_DECL_PUSH_CONSTANT(ComputeRasterizePush, push)

DAXA_DECL_IMAGE_ACCESSOR_WITH_FORMAT(u64image2D, r64ui, , r64uiImage)

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

#define u_visbuffer daxa_access(r64uiImage, push.uses.visbuffer64)
#define SUBPIXEL_BITS 12
#define SUBPIXEL_SAMPLES (1 << SUBPIXEL_BITS)

DAXA_STORAGE_IMAGE_LAYOUT_WITH_FORMAT(r32ui)
uniform uimage2D atomic_u32_table[];

void write_pixel(ivec2 p, uint64_t payload, float depth) {
    imageAtomicMax(daxa_access(r64uiImage, push.uses.visbuffer64), p, payload | (uint64_t(floatBitsToUint(depth)) << 32));
#if ENABLE_DEBUG_VIS
    imageAtomicAdd(atomic_u32_table[daxa_image_view_id_to_index(push.uses.debug_overdraw)], p, 1);
#endif
}

#include <renderer/rasterize.glsl>

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

        mat4 world_to_clip = deref(push.uses.gpu_input).cam.view_to_sample * deref(push.uses.gpu_input).cam.world_to_view;

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
        if (is_ndc_aabb_hiz_depth_occluded(ndc_min, ndc_max, deref(push.uses.gpu_input).render_size, deref(push.uses.gpu_input).next_lower_po2_render_size, push.uses.hiz)) {
            return;
        }
#endif

        p0.xy = floor(p0.xy * scale + bias);
        p1.xy = floor(p1.xy * scale + bias);
        p2.xy = floor(p2.xy * scale + bias);
        p3.xy = floor(p3.xy * scale + bias);

#if DRAW_FROM_OBSERVER
        {
            vec3 v01 = p1 - p0;
            vec3 v02 = p2 - p0;
            float det_xy = v01.x * v02.y - v01.y * v02.x;
            if (det_xy >= 0.0) {
                return;
            }

            float inv_det = 1.0 / det_xy;
            vec2 grad_z = vec2(
                (v01.z * v02.y - v01.y * v02.z) * inv_det,
                (v01.x * v02.z - v01.z * v02.x) * inv_det);

            vec2 vert_0 = p0.xy;
            vec2 vert_1 = p1.xy;
            vec2 vert_2 = p2.xy;
            vec2 vert_3 = p3.xy;

            vec2 min_subpixel = min(min(vert_0, vert_1), min(vert_2, vert_3));
            vec2 max_subpixel = max(max(vert_0, vert_1), max(vert_2, vert_3));

            ivec2 min_pixel = ivec2(floor((min_subpixel + (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / float(SUBPIXEL_SAMPLES))));
            ivec2 max_pixel = ivec2(floor((max_subpixel - (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / float(SUBPIXEL_SAMPLES))));

            min_pixel = max(min_pixel, ivec2(0));
            max_pixel = min(max_pixel, viewport_size.xy - 1);
            if (any(greaterThan(min_pixel, max_pixel))) {
                return;
            }

            mat4 main_clip_to_observer_clip =
                deref(push.uses.gpu_input).observer_cam.view_to_sample * deref(push.uses.gpu_input).observer_cam.world_to_view *
                deref(push.uses.gpu_input).cam.view_to_world * deref(push.uses.gpu_input).cam.sample_to_view;
            vec4 temp0 = main_clip_to_observer_clip * vec4((p0.xy - bias) / scale, p0.z, 1);
            vec4 temp1 = main_clip_to_observer_clip * vec4((p1.xy - bias) / scale, p1.z, 1);
            vec4 temp2 = main_clip_to_observer_clip * vec4((p2.xy - bias) / scale, p2.z, 1);
            vec4 temp3 = main_clip_to_observer_clip * vec4((p3.xy - bias) / scale, p3.z, 1);

            temp0 = temp0 / temp0.w;
            temp1 = temp1 / temp1.w;
            temp2 = temp2 / temp2.w;
            temp3 = temp3 / temp3.w;

            if (temp0.z < 0 || temp1.z < 0 || temp2.z < 0 || temp3.z < 0) {
                return;
            }

            temp0.xy = floor(temp0.xy * scale + bias);
            temp1.xy = floor(temp1.xy * scale + bias);
            temp2.xy = floor(temp2.xy * scale + bias);
            temp3.xy = floor(temp3.xy * scale + bias);

            det_xy = (temp1.x - temp0.x) * (temp2.y - temp0.y) - (temp1.y - temp0.y) * (temp2.x - temp0.x);
            if (det_xy >= 0.0) {
                // if we're a back-face, we want to swap the winding order, since we only want to backface cull for
                // the backfaces relative to the main camera.
                vec4 temp = temp1;
                temp1 = temp2;
                temp2 = temp;
            }

            p0 = temp0.xyz;
            p1 = temp1.xyz;
            p2 = temp2.xyz;
            p3 = temp3.xyz;
        }
#endif

        rasterize_quad(vec3[](p0, p1, p2, p3), viewport_size, uint64_t(o_packed_payload.data));
    }
}
