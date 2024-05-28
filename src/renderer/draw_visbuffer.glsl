#include <shared.inl>
#include <renderer/visbuffer.glsl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/meshlet_allocator.glsl>

#define DISCARD_METHOD 0

DAXA_DECL_PUSH_CONSTANT(DrawVisbufferPush, push)

struct TaskPayload {
    uint face_count;
    uint brick_instance_index;
};
struct PackedTaskPayload {
    uint data;
};

PackedTaskPayload pack(TaskPayload payload) {
    PackedTaskPayload result;
    result.data = payload.face_count & 0xfff;
    result.data |= (payload.brick_instance_index & 0xfffff) << 12;
    return result;
}

TaskPayload unpack(PackedTaskPayload packed_payload) {
    TaskPayload result;
    result.face_count = packed_payload.data & 0xfff;
    result.brick_instance_index = (packed_payload.data >> 12) & 0xfffff;
    return result;
}

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TASK
#extension GL_EXT_mesh_shader : enable

taskPayloadSharedEXT PackedTaskPayload packed_payload;

layout(local_size_x = 32) in;
void main() {
    uint brick_instance_index = gl_WorkGroupID.x + 1;

    if (!is_valid_index(daxa_BufferPtr(BrickInstance)(push.uses.brick_instance_allocator), brick_instance_index)) {
        return;
    }

    BrickInstance brick_instance = deref(push.uses.brick_instance_allocator[brick_instance_index]);
    VoxelChunk voxel_chunk = deref(push.uses.chunks[brick_instance.chunk_index]);
    VoxelBrickMesh mesh = deref(voxel_chunk.meshes[brick_instance.brick_index]);

    if (gl_LocalInvocationIndex == 0) {
        TaskPayload payload;
        payload.face_count = mesh.face_count;
        payload.brick_instance_index = brick_instance_index;
        packed_payload = pack(payload);
        EmitMeshTasksEXT((mesh.face_count + 31) / 32, 1, 1);
    }
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_MESH
#extension GL_EXT_mesh_shader : enable

#include <renderer/hiz.glsl>

taskPayloadSharedEXT PackedTaskPayload packed_payload;
layout(triangles) out;
layout(max_vertices = MAX_VERTICES_PER_MESHLET, max_primitives = MAX_TRIANGLES_PER_MESHLET) out;

layout(location = 0) out flat PackedVisbufferPayload v_payload[];
#if DISCARD_METHOD
layout(location = 1) out vec2 v_uv[];
#endif

mat4 world_to_clip;
PackedVisbufferPayload o_packed_payload;

bool is_micropoly_invisible(vec2 ndc_min, vec2 ndc_max, vec2 resolution) {
    // Cope epsilon to be conservative
    const float EPS = 1.0 / 256.0f;
    vec2 sample_grid_min = (ndc_min * 0.5f + 0.5f) * resolution - 0.5f - EPS;
    vec2 sample_grid_max = (ndc_max * 0.5f + 0.5f) * resolution - 0.5f + EPS;
    // Checks if the min and the max positions are right next to the same sample grid line.
    // If we are next to the same sample grid line in one dimension we are not rasterized.
    return any(equal(floor(sample_grid_max), floor(sample_grid_min))) || outside_frustum(ndc_min, ndc_max);
}

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

    bool micro_poly_invisible = is_micropoly_invisible(ndc_min.xy, ndc_max.xy, vec2(deref(push.uses.gpu_input).render_size));
    bool facing_away = determinant(mat3(p0_h.xyw, p1_h.xyw, p2_h.xyw)) <= 0;
    uint face_id = gl_LocalInvocationIndex;

#if DO_DEPTH_CULL
    const bool depth_occluded = is_ndc_aabb_hiz_depth_occluded(ndc_min, ndc_max, deref(push.uses.gpu_input).next_lower_po2_render_size, push.uses.hiz);
    bool cull_poly = micro_poly_invisible || facing_away || depth_occluded;
#else
    bool cull_poly = micro_poly_invisible || facing_away;
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

void emit_prim_x(vec3 pos, vec2 size, int flip) {
#if DISCARD_METHOD
    float winding_flip_a = flip * 2.0;
    float winding_flip_b = 2.0 - winding_flip_a;
    vec3 p0 = pos + vec3(0, -size.x * 0.0, -size.y * 0.0);
    vec3 p1 = pos + vec3(0, size.x * winding_flip_a, size.y * winding_flip_b);
    vec3 p2 = pos + vec3(0, size.x * winding_flip_b, size.y * winding_flip_a);
    vec3 p3 = vec3(0);
#else
    float winding_flip_a = flip;
    float winding_flip_b = 1.0 - winding_flip_a;
    vec3 p0 = pos + vec3(0, -size.x * 0.0, -size.y * 0.0);
    vec3 p1 = pos + vec3(0, size.x * winding_flip_a, size.y * winding_flip_b);
    vec3 p2 = pos + vec3(0, size.x * winding_flip_b, size.y * winding_flip_a);
    vec3 p3 = pos + vec3(0, +size.x * 1.0, +size.y * 1.0);
#endif
    emit_prim(p0, p1, p2, p3);
}

void emit_prim_y(vec3 pos, vec2 size, int flip) {
#if DISCARD_METHOD
    float winding_flip_a = flip * 2.0;
    float winding_flip_b = 2.0 - winding_flip_a;
    vec3 p0 = pos + vec3(-size.x * 0.0, 0, -size.y * 0.0);
    vec3 p1 = pos + vec3(size.x * winding_flip_b, 0, size.y * winding_flip_a);
    vec3 p2 = pos + vec3(size.x * winding_flip_a, 0, size.y * winding_flip_b);
    vec3 p3 = vec3(0);
#else
    float winding_flip_a = flip;
    float winding_flip_b = 1.0 - winding_flip_a;
    vec3 p0 = pos + vec3(-size.x * 0.0, 0, -size.y * 0.0);
    vec3 p1 = pos + vec3(size.x * winding_flip_b, 0, size.y * winding_flip_a);
    vec3 p2 = pos + vec3(size.x * winding_flip_a, 0, size.y * winding_flip_b);
    vec3 p3 = pos + vec3(+size.x * 1.0, 0, +size.y * 1.0);
#endif
    emit_prim(p0, p1, p2, p3);
}

void emit_prim_z(vec3 pos, vec2 size, int flip) {
#if DISCARD_METHOD
    float winding_flip_a = flip * 2.0;
    float winding_flip_b = 2.0 - winding_flip_a;
    vec3 p0 = pos + vec3(-size.x * 0.0, -size.y * 0.0, 0);
    vec3 p1 = pos + vec3(size.x * winding_flip_a, size.y * winding_flip_b, 0);
    vec3 p2 = pos + vec3(size.x * winding_flip_b, size.y * winding_flip_a, 0);
    vec3 p3 = vec3(0);
#else
    float winding_flip_a = flip;
    float winding_flip_b = 1.0 - winding_flip_a;
    vec3 p0 = pos + vec3(-size.x * 0.0, -size.y * 0.0, 0);
    vec3 p1 = pos + vec3(size.x * winding_flip_a, size.y * winding_flip_b, 0);
    vec3 p2 = pos + vec3(size.x * winding_flip_b, size.y * winding_flip_a, 0);
    vec3 p3 = pos + vec3(+size.x * 1.0, +size.y * 1.0, 0);
#endif
    emit_prim(p0, p1, p2, p3);
}

layout(local_size_x = 32) in;
void main() {
    world_to_clip = deref(push.uses.gpu_input).cam.view_to_clip * deref(push.uses.gpu_input).cam.world_to_view;
    TaskPayload payload = unpack(packed_payload);

    uint meshlet_index = gl_WorkGroupID.x;
    uint in_meshlet_face_index = gl_LocalInvocationIndex;
    uint meshlet_face_count = min(32, payload.face_count - meshlet_index * 32);

#if DISCARD_METHOD
    SetMeshOutputsEXT(meshlet_face_count * 3, meshlet_face_count * 1);
#else
    SetMeshOutputsEXT(meshlet_face_count * 4, meshlet_face_count * 2);
#endif

    BrickInstance brick_instance = deref(push.uses.brick_instance_allocator[payload.brick_instance_index]);
    VoxelChunk voxel_chunk = deref(push.uses.chunks[brick_instance.chunk_index]);
    VoxelBrickMesh mesh = deref(voxel_chunk.meshes[brick_instance.brick_index]);

    if (in_meshlet_face_index < meshlet_face_count && mesh.meshlet_start != 0) {
        VoxelMeshlet meshlet = deref(push.uses.meshlet_allocator[meshlet_index + mesh.meshlet_start]);
        PackedVoxelBrickFace packed_face = meshlet.faces[gl_LocalInvocationIndex];

        VoxelBrickFace face = unpack(packed_face);

        VisbufferPayload o_payload;
        o_payload.face_id = gl_LocalInvocationIndex;
        o_payload.meshlet_id = meshlet_index + mesh.meshlet_start;
        o_packed_payload = pack(o_payload);

        ivec4 pos_scl = deref(voxel_chunk.pos_scl[brick_instance.brick_index]);
        ivec3 pos = ivec3(voxel_chunk.pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(face.pos);
        int scl = pos_scl.w + 8;

#define SCL (float(1 << scl) / float(1 << 8))
        switch (face.axis / 2) {
        case 0:
            pos.x += int(face.axis % 2);
            emit_prim_x(vec3(pos) * SCL, vec2(SCL), int(face.axis % 2));
            break;
        case 1:
            pos.y += int(face.axis % 2);
            emit_prim_y(vec3(pos) * SCL, vec2(SCL), int(face.axis % 2));
            break;
        case 2:
            pos.z += int(face.axis % 2);
            emit_prim_z(vec3(pos) * SCL, vec2(SCL), int(face.axis % 2));
            break;
        }
    }
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

DAXA_STORAGE_IMAGE_LAYOUT_WITH_FORMAT(r32ui)
uniform uimage2D atomic_u32_table[];

layout(location = 0) in flat PackedVisbufferPayload v_payload;
#if DISCARD_METHOD
layout(location = 1) in vec2 v_uv;
#endif
layout(location = 0) out uvec4 f_out;
void main() {
#if DISCARD_METHOD
    if (any(greaterThan(v_uv, vec2(1.0)))) {
        discard;
    }
#endif

#if ENABLE_DEBUG_VIS
    imageAtomicAdd(atomic_u32_table[daxa_image_view_id_to_index(push.uses.debug_overdraw)], ivec2(gl_FragCoord.xy), 1);
#endif

    f_out = uvec4(v_payload.data, 0, 0, 0);
}

#endif
