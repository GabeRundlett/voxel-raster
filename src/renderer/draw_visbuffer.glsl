#include <shared.inl>
#include <renderer/visbuffer.glsl>
#include <voxels/voxel_mesh.glsl>

#define DISCARD_METHOD 0

DAXA_DECL_PUSH_CONSTANT(DrawVisbufferPush, push)

struct TaskPayload {
    uint face_count;
    uint brick_id;
};
struct PackedTaskPayload {
    uint data;
};

PackedTaskPayload pack(TaskPayload payload) {
    PackedTaskPayload result;
    result.data = payload.face_count & 0xfff;
    result.data |= (payload.brick_id & 0xfffff) << 12;
    return result;
}

TaskPayload unpack(PackedTaskPayload packed_payload) {
    TaskPayload result;
    result.face_count = packed_payload.data & 0xfff;
    result.brick_id = (packed_payload.data >> 12) & 0xfffff;
    return result;
}

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TASK
#extension GL_EXT_mesh_shader : enable

taskPayloadSharedEXT PackedTaskPayload packed_payload;

layout(local_size_x = 32) in;
void main() {
    uint brick_id = gl_WorkGroupID.x;
    if (brick_id >= deref(push.uses.gpu_input).brick_n) {
        return;
    }

    VoxelBrickMesh mesh = deref(push.uses.meshes[brick_id]);

    if (gl_LocalInvocationIndex == 0) {
        TaskPayload payload;
        payload.face_count = mesh.face_count;
        payload.brick_id = brick_id;
        packed_payload = pack(payload);
        EmitMeshTasksEXT((mesh.face_count + 31) / 32, 1, 1);
    }
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_MESH
#extension GL_EXT_mesh_shader : enable

taskPayloadSharedEXT PackedTaskPayload packed_payload;
layout(triangles) out;
layout(max_vertices = MAX_VERTICES_PER_MESHLET, max_primitives = MAX_TRIANGLES_PER_MESHLET) out;

layout(location = 0) out flat PackedVisbufferPayload v_payload[];
#if DISCARD_METHOD
layout(location = 1) out vec2 v_uv[];
#endif

shared uint current_vert_n;
shared uint current_prim_n;

mat4 world_to_clip;
PackedVisbufferPayload o_packed_payload;

bool is_micropoly_visible(vec2 ndc_min, vec2 ndc_max, vec2 resolution) {
    // Cope epsilon to be conservative
    const float EPS = 1.0 / 256.0f;
    vec2 sample_grid_min = (ndc_min * 0.5f + 0.5f) * resolution - 0.5f - EPS;
    vec2 sample_grid_max = (ndc_max * 0.5f + 0.5f) * resolution - 0.5f + EPS;
    // Checks if the min and the max positions are right next to the same sample grid line.
    // If we are next to the same sample grid line in one dimension we are not rasterized.
    bool prim_visible = !any(equal(floor(sample_grid_max), floor(sample_grid_min)));
    return prim_visible && !(any(greaterThan(ndc_min, vec2(1))) || any(lessThan(ndc_max, vec2(-1))));
}

void emit_prim(vec3 pos, vec2 size) {
#if DISCARD_METHOD
    vec4 p0_h = world_to_clip * vec4(pos + vec3(-size.x * 0.5, -size.y * 0.5, 0), 1);
    vec4 p1_h = world_to_clip * vec4(pos + vec3(+size.x * 1.5, -size.y * 0.5, 0), 1);
    vec4 p2_h = world_to_clip * vec4(pos + vec3(-size.x * 0.5, +size.y * 1.5, 0), 1);

    vec2 p0 = p0_h.xy / p0_h.w;
    vec2 p1 = p1_h.xy / p1_h.w;
    vec2 p2 = p2_h.xy / p2_h.w;
    vec2 ndc_min = min(min(p0, p1), p2);
    vec2 ndc_max = max(max(p0, p1), p2);
    bool micro_poly_visible = is_micropoly_visible(ndc_min, ndc_max, vec2(deref(push.uses.gpu_input).render_size));

    if (micro_poly_visible) {
        uint vert_i = atomicAdd(current_vert_n, 3);
        uint prim_i = atomicAdd(current_prim_n, 1);

        gl_MeshPrimitivesEXT[prim_i].gl_CullPrimitiveEXT = false;
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
    vec4 p0_h = world_to_clip * vec4(pos + vec3(-size.x * 0.5, -size.y * 0.5, 0), 1);
    vec4 p1_h = world_to_clip * vec4(pos + vec3(+size.x * 0.5, -size.y * 0.5, 0), 1);
    vec4 p2_h = world_to_clip * vec4(pos + vec3(-size.x * 0.5, +size.y * 0.5, 0), 1);
    vec4 p3_h = world_to_clip * vec4(pos + vec3(+size.x * 0.5, +size.y * 0.5, 0), 1);

    vec2 p0 = p0_h.xy / p0_h.w;
    vec2 p1 = p1_h.xy / p1_h.w;
    vec2 p2 = p2_h.xy / p2_h.w;
    vec2 p3 = p3_h.xy / p3_h.w;
    vec2 ndc_min = min(min(p0, p1), min(p2, p3));
    vec2 ndc_max = max(max(p0, p1), max(p2, p3));
    bool micro_poly_visible = is_micropoly_visible(ndc_min, ndc_max, vec2(deref(push.uses.gpu_input).render_size));

    if (micro_poly_visible) {
        uint vert_i = atomicAdd(current_vert_n, 4);
        uint prim_i = atomicAdd(current_prim_n, 2);

        gl_MeshPrimitivesEXT[prim_i + 0].gl_CullPrimitiveEXT = false;
        gl_PrimitiveTriangleIndicesEXT[prim_i + 0] = uvec3(0, 1, 2) + vert_i;

        gl_MeshPrimitivesEXT[prim_i + 1].gl_CullPrimitiveEXT = false;
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

void begin_prim() {
    if (gl_LocalInvocationIndex == 0) {
        current_vert_n = 0;
        current_prim_n = 0;
    }
    barrier();
}
void end_prim() {
    barrier();
    SetMeshOutputsEXT(current_vert_n, current_prim_n);
}

layout(local_size_x = 32) in;
void main() {
    world_to_clip = deref(push.uses.gpu_input).cam.view_to_clip * deref(push.uses.gpu_input).cam.world_to_view;
    TaskPayload payload = unpack(packed_payload);

    begin_prim();

    if (gl_GlobalInvocationID.x < payload.face_count) {
        uint voxel_id = gl_GlobalInvocationID.x;

        int xi = int(voxel_id % VOXEL_BRICK_SIZE);
        int yi = int((voxel_id / VOXEL_BRICK_SIZE) % VOXEL_BRICK_SIZE);
        int zi = int(voxel_id / VOXEL_BRICK_SIZE / VOXEL_BRICK_SIZE);

        VisbufferPayload o_payload;
        o_payload.brick_id = payload.brick_id;
        o_payload.voxel_id = voxel_id;
        o_packed_payload = pack(o_payload);

        ivec4 pos_scl = deref(push.uses.pos_scl[payload.brick_id]);
        ivec3 pos = pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(xi, yi, zi);
        int scl = pos_scl.w;

#define SCL (1.0 / VOXEL_BRICK_SIZE * (1 << scl))
        emit_prim(vec3(pos) * SCL, vec2(SCL));
    }

    end_prim();
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

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
    f_out = uvec4(v_payload.data, 0, 0, 0);
}

#endif
