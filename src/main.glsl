#include <shared.inl>

#define DISCARD_METHOD 0

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TASK
#extension GL_EXT_mesh_shader : enable

layout(local_size_x = 32) in;
void main() {
    // emit 1 mesh shader
    if (gl_GlobalInvocationID.x != 0) {
        return;
    }
    EmitMeshTasksEXT((100 + 31) / 32, 100, 1);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_MESH
#extension GL_EXT_mesh_shader : enable

layout(triangles) out;
layout(max_vertices = 128, max_primitives = 126) out;

layout(location = 0) out vec2 v_uv[];

shared uint current_vert_n;
shared uint current_prim_n;
void emit_prim(vec3 pos, vec2 size) {
#if DISCARD_METHOD
    uint vert_i = atomicAdd(current_vert_n, 3);
    uint prim_i = atomicAdd(current_prim_n, 1);

    gl_MeshPrimitivesEXT[prim_i].gl_CullPrimitiveEXT = false;
    gl_PrimitiveTriangleIndicesEXT[prim_i] = uvec3(0, 1, 2) + vert_i;

    v_uv[vert_i + 0] = vec2(0, 0);
    v_uv[vert_i + 1] = vec2(2, 0);
    v_uv[vert_i + 2] = vec2(0, 2);

    gl_MeshVerticesEXT[vert_i + 0].gl_Position = vec4(pos + vec3(-size.x * 0.5, -size.y * 0.5, 0), 1);
    gl_MeshVerticesEXT[vert_i + 1].gl_Position = vec4(pos + vec3(+size.x * 1.5, -size.y * 0.5, 0), 1);
    gl_MeshVerticesEXT[vert_i + 2].gl_Position = vec4(pos + vec3(-size.x * 0.5, +size.y * 1.5, 0), 1);
#else
    uint vert_i = atomicAdd(current_vert_n, 4);
    uint prim_i = atomicAdd(current_prim_n, 2);

    gl_MeshPrimitivesEXT[prim_i + 0].gl_CullPrimitiveEXT = false;
    gl_PrimitiveTriangleIndicesEXT[prim_i + 0] = uvec3(0, 1, 2) + vert_i;

    gl_MeshPrimitivesEXT[prim_i + 1].gl_CullPrimitiveEXT = false;
    gl_PrimitiveTriangleIndicesEXT[prim_i + 1] = uvec3(1, 2, 3) + vert_i;

    v_uv[vert_i + 0] = vec2(0, 0);
    v_uv[vert_i + 1] = vec2(1, 0);
    v_uv[vert_i + 2] = vec2(0, 1);
    v_uv[vert_i + 3] = vec2(1, 1);

    gl_MeshVerticesEXT[vert_i + 0].gl_Position = vec4(pos + vec3(-size.x * 0.5, -size.y * 0.5, 0), 1);
    gl_MeshVerticesEXT[vert_i + 1].gl_Position = vec4(pos + vec3(+size.x * 0.5, -size.y * 0.5, 0), 1);
    gl_MeshVerticesEXT[vert_i + 2].gl_Position = vec4(pos + vec3(-size.x * 0.5, +size.y * 0.5, 0), 1);
    gl_MeshVerticesEXT[vert_i + 3].gl_Position = vec4(pos + vec3(+size.x * 0.5, +size.y * 0.5, 0), 1);
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
    begin_prim();

    int xi = int(gl_LocalInvocationID.x + gl_WorkGroupID.x * 32);
    int yi = int(gl_WorkGroupID.y);
    emit_prim(vec3((xi + 1) * 0.02 - 1, (yi + 1) * 0.02 - 1, 0), vec2(0.015));

    end_prim();
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in vec2 v_uv;
layout(location = 0) out daxa_f32vec4 color;
void main() {
#if DISCARD_METHOD
    if (any(greaterThan(v_uv, vec2(1.0)))) {
        discard;
    }
#endif
    color = daxa_f32vec4(vec3(v_uv, 0), 1);
}

#endif
