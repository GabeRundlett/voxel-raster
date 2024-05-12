#include <shared.inl>

#define DISCARD_METHOD 0

DAXA_DECL_PUSH_CONSTANT(DrawVisbufferPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TASK
#extension GL_EXT_mesh_shader : enable

layout(local_size_x = 32) in;
void main() {
    // emit 1 mesh shader
    if (gl_GlobalInvocationID.x != 0) {
        return;
    }
    EmitMeshTasksEXT((3200 + 31) / 32, 3200, 1);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_MESH
#extension GL_EXT_mesh_shader : enable

layout(triangles) out;
layout(max_vertices = 128, max_primitives = 126) out;

layout(location = 0) out flat uint v_id[];
#if DISCARD_METHOD
layout(location = 1) out vec2 v_uv[];
#endif

shared uint current_vert_n;
shared uint current_prim_n;

mat4 world_to_clip;
uint packed_id;

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

        v_id[vert_i + 0] = packed_id;
        v_id[vert_i + 1] = packed_id;
        v_id[vert_i + 2] = packed_id;

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

        v_id[vert_i + 0] = packed_id;
        v_id[vert_i + 1] = packed_id;
        v_id[vert_i + 2] = packed_id;
        v_id[vert_i + 3] = packed_id;

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

    begin_prim();

    int xi = int(gl_LocalInvocationID.x + gl_WorkGroupID.x * 32);
    int yi = int(gl_WorkGroupID.y);
    int zi = int(gl_WorkGroupID.z);

    packed_id = xi + (yi << 10) + (zi << 20);

    emit_prim(vec3((xi + 1) * 0.0125 - 1, (yi + 1) * 0.0125 - 1, (zi + 1) * 0.0125 - 1), vec2(0.0125));

    end_prim();
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in flat uint v_id;
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
    f_out = uvec4(v_id, 0, 0, 0);
}

#endif
