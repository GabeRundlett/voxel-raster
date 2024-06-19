#include "voxel_rasterizer.inl"
#include <voxels/voxel_mesh.glsl>
#include "visbuffer.glsl"
#include "allocators.glsl"

#define DISCARD_METHOD 0

DAXA_DECL_PUSH_CONSTANT(DrawVisbufferPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_MESH
#extension GL_EXT_mesh_shader : enable

#include "culling.glsl"

layout(triangles) out;
layout(max_vertices = MAX_VERTICES_PER_MESHLET, max_primitives = MAX_TRIANGLES_PER_MESHLET) out;

layout(location = 0) out flat PackedVisbufferPayload v_payload[];
#if DISCARD_METHOD
layout(location = 1) out vec2 v_uv[];
#endif

mat4 world_to_clip;
PackedVisbufferPayload o_packed_payload;

void emit_prim(vec3 in_p0, vec3 in_p1, vec3 in_p2, vec3 in_p3) {
    vec4 p0_h = world_to_clip * vec4(in_p0, 1);
    vec4 p1_h = world_to_clip * vec4(in_p1, 1);
    vec4 p2_h = world_to_clip * vec4(in_p2, 1);

    vec3 p0 = p0_h.xyz / p0_h.w;
    vec3 p1 = p1_h.xyz / p1_h.w;
    vec3 p2 = p2_h.xyz / p2_h.w;

#if DISCARD_METHOD
    vec3 ndc_min = min(min(p0, p1), p2);
    vec3 ndc_max = max(max(p0, p1), p2);
#else
    vec4 p3_h = world_to_clip * vec4(in_p3, 1);
    vec3 p3 = p3_h.xyz / p3_h.w;
    vec3 ndc_min = min(min(p0, p1), min(p2, p3));
    vec3 ndc_max = max(max(p0, p1), max(p2, p3));
#endif

    const vec2 v01 = p1.xy - p0.xy;
    const vec2 v02 = p2.xy - p0.xy;
    const float det_xy = v01.x * v02.y - v01.y * v02.x;

    bool between_raster_grid_lines = is_between_raster_grid_lines(ndc_min.xy, ndc_max.xy, vec2(deref(push.uses.gpu_input).render_size));
    bool facing_away = det_xy >= 0.0;
    bool outside_frustum = is_outside_frustum(ndc_min.xy, ndc_max.xy);
    uint face_id = gl_LocalInvocationIndex;

#if DO_DEPTH_CULL
    const bool depth_occluded = is_ndc_aabb_hiz_depth_occluded(ndc_min, ndc_max, deref(push.uses.gpu_input).render_size, deref(push.uses.gpu_input).next_lower_po2_render_size, push.uses.hiz);
    bool cull_poly = outside_frustum || between_raster_grid_lines || facing_away || depth_occluded;
#else
    bool cull_poly = outside_frustum || between_raster_grid_lines || facing_away;
#endif

#if DISCARD_METHOD
    uint vert_i = face_id * 3;
    uint prim_i = face_id * 1;
    gl_MeshPrimitivesEXT[prim_i].gl_CullPrimitiveEXT = cull_poly;
    if (!cull_poly) {
        gl_PrimitiveTriangleIndicesEXT[prim_i] = uvec3(0, 1, 2) + vert_i;
        v_uv[vert_i + 0] = vec2(0, 0);
        v_uv[vert_i + 1] = vec2(2, 0);
        v_uv[vert_i + 2] = vec2(0, 2);
        v_payload[vert_i + 0] = o_packed_payload;
        v_payload[vert_i + 1] = o_packed_payload;
        v_payload[vert_i + 2] = o_packed_payload;
        gl_MeshVerticesEXT[vert_i + 0].gl_Position = p0_h;
        gl_MeshVerticesEXT[vert_i + 1].gl_Position = p1_h;
        gl_MeshVerticesEXT[vert_i + 2].gl_Position = p2_h;
    }
#else
    uint vert_i = face_id * 4;
    uint prim_i = face_id * 2;
    gl_MeshPrimitivesEXT[prim_i + 0].gl_CullPrimitiveEXT = cull_poly;
    gl_MeshPrimitivesEXT[prim_i + 1].gl_CullPrimitiveEXT = cull_poly;

#if DRAW_FROM_OBSERVER
    world_to_clip = deref(push.uses.gpu_input).observer_cam.view_to_sample * deref(push.uses.gpu_input).observer_cam.world_to_view;

    p0_h = world_to_clip * vec4(in_p0, 1);
    p1_h = world_to_clip * vec4(in_p1, 1);
    p2_h = world_to_clip * vec4(in_p2, 1);
#if !DISCARD_METHOD
    p3_h = world_to_clip * vec4(in_p3, 1);
#endif
#endif

    if (!cull_poly) {
        gl_PrimitiveTriangleIndicesEXT[prim_i + 0] = uvec3(0, 1, 2) + vert_i;
        gl_PrimitiveTriangleIndicesEXT[prim_i + 1] = uvec3(1, 2, 3) + vert_i;
        v_payload[vert_i + 0] = o_packed_payload;
        v_payload[vert_i + 1] = o_packed_payload;
        v_payload[vert_i + 2] = o_packed_payload;
        v_payload[vert_i + 3] = o_packed_payload;
        gl_MeshVerticesEXT[vert_i + 0].gl_Position = p0_h;
        gl_MeshVerticesEXT[vert_i + 1].gl_Position = p1_h;
        gl_MeshVerticesEXT[vert_i + 2].gl_Position = p2_h;
        gl_MeshVerticesEXT[vert_i + 3].gl_Position = p3_h;
    }
#endif
}

layout(local_size_x = 32) in;
void main() {
    world_to_clip = deref(push.uses.gpu_input).cam.view_to_sample * deref(push.uses.gpu_input).cam.world_to_view;
    BrickInstance brick_instance;
    VoxelChunk voxel_chunk;
    VoxelBrickMesh mesh;

    uint in_mesh_meshlet_index = 0;
    uint in_meshlet_face_index = gl_LocalInvocationIndex;
    uint meshlet_face_count = 0;

    uint meshlet_index = gl_WorkGroupID.x + 1 + deref(push.uses.indirect_info[3]).offset + MAX_SW_MESHLET_COUNT;
    if (is_valid_meshlet_index(daxa_BufferPtr(VoxelMeshlet)(push.uses.meshlet_allocator), meshlet_index)) {
        VoxelMeshletMetadata metadata = deref(push.uses.meshlet_metadata[meshlet_index]);

        if (is_valid_index(daxa_BufferPtr(BrickInstance)(push.uses.brick_instance_allocator), metadata.brick_instance_index)) {
            brick_instance = deref(push.uses.brick_instance_allocator[metadata.brick_instance_index]);
            voxel_chunk = deref(push.uses.chunks[brick_instance.chunk_index]);
            mesh = deref(voxel_chunk.meshes[brick_instance.brick_index]);
            in_mesh_meshlet_index = meshlet_index - mesh.meshlet_start;
            meshlet_face_count = min(32, mesh.face_count - in_mesh_meshlet_index * 32);
        }
    }

#if DISCARD_METHOD
    SetMeshOutputsEXT(meshlet_face_count * 3, meshlet_face_count * 1);
#else
    SetMeshOutputsEXT(meshlet_face_count * 4, meshlet_face_count * 2);
#endif

    if (in_meshlet_face_index < meshlet_face_count && mesh.meshlet_start != 0) {
        PackedVoxelBrickFace packed_face = deref(push.uses.meshlet_allocator[in_mesh_meshlet_index + mesh.meshlet_start]).faces[gl_LocalInvocationIndex];

        VoxelBrickFace face = unpack(packed_face);

        VisbufferPayload o_payload;
        o_payload.face_id = in_meshlet_face_index;
        o_payload.meshlet_id = meshlet_index;
        o_packed_payload = pack(o_payload);

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

#if DISCARD_METHOD
        int winding_flip_a = flip * 2;
        int winding_flip_b = 2 - winding_flip_a;
#else
        int winding_flip_a = flip;
        int winding_flip_b = 1 - winding_flip_a;
#endif
        ivec3 p0 = pos;
        ivec3 p1 = pos + ivec3(int(axis != 0) * winding_flip_a, int(axis == 0) * winding_flip_a + int(axis == 2) * winding_flip_b, int(axis != 2) * winding_flip_b);
        ivec3 p2 = pos + ivec3(int(axis != 0) * winding_flip_b, int(axis == 0) * winding_flip_b + int(axis == 2) * winding_flip_a, int(axis != 2) * winding_flip_a);
        ivec3 p3 = pos + ivec3(int(axis != 0), int(axis != 1), int(axis != 2));
        emit_prim(p0 * SCL, p1 * SCL, p2 * SCL, p3 * SCL);
    }
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

DAXA_DECL_IMAGE_ACCESSOR_WITH_FORMAT(u64image2D, r64ui, , r64uiImage)
DAXA_STORAGE_IMAGE_LAYOUT_WITH_FORMAT(r32ui)
uniform uimage2D atomic_u32_table[];

void write_pixel(ivec2 p, uint64_t payload, float depth) {
    imageAtomicMax(daxa_access(r64uiImage, push.uses.visbuffer64), p, payload | (uint64_t(floatBitsToUint(depth)) << 32));
#if ENABLE_DEBUG_VIS
    imageAtomicAdd(atomic_u32_table[daxa_image_view_id_to_index(push.uses.debug_overdraw)], p, 1);
#endif
}

layout(location = 0) in flat PackedVisbufferPayload v_payload;
#if DISCARD_METHOD
layout(location = 1) in vec2 v_uv;
#endif
// layout(location = 0) out uvec4 f_out;
void main() {
#if DISCARD_METHOD
    if (any(greaterThan(v_uv, vec2(1.0)))) {
        discard;
    }
#endif

    write_pixel(ivec2(gl_FragCoord.xy), v_payload.data, gl_FragCoord.z);
}

#endif
