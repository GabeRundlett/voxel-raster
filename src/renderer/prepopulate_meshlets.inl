#pragma once

#include <renderer/shared.inl>

#define ALLOC_ENT_TO_MESH_INST_OFFSETS_OFFSETS_X 128
#define PREPOPULATE_MESHLET_INSTANCES_X 256

DAXA_DECL_TASK_HEAD_BEGIN(AllocEntToMeshInstOffsetsOffsetsH)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE_CONCURRENT, daxa_BufferPtr(RenderGlobalData), globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(OpaqueMeshInstancesBufferHead), opaque_mesh_instances)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(daxa_u32), entity_mesh_groups)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GPUMeshGroup), mesh_groups)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(IndirectMemsetBufferCommand), clear_arena_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(daxa_u32), ent_to_mesh_inst_offsets_offsets)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, U32ArenaBufferRef, bitfield_arena)
DAXA_DECL_TASK_HEAD_END

struct AllocEntToMeshInstOffsetsOffsetsPush {
    DAXA_TH_BLOB(AllocEntToMeshInstOffsetsOffsetsH, uses)
};

DAXA_DECL_TASK_HEAD_BEGIN(PrepopMeshletInstancesCommWH)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE_CONCURRENT, daxa_BufferPtr(RenderGlobalData), globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VisibleMeshletList), visible_meshlets_prev)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_RWBufferPtr(DispatchIndirectStruct), command)
DAXA_DECL_TASK_HEAD_END

struct PrepopMeshletInstancesCommWPush {
    DAXA_TH_BLOB(PrepopMeshletInstancesCommWH, uses)
};

// - Goes over all visible meshlets from last frame
// - Attempts to allocate a meshlet instance bitfield offset for each mesh in the list of visible meshlets
DAXA_DECL_TASK_HEAD_BEGIN(AllocMeshletInstBitfieldsH)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE_CONCURRENT, daxa_BufferPtr(RenderGlobalData), globals)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ, command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VisibleMeshletList), visible_meshlets_prev)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletInstancesBufferHead), meshlet_instances_prev)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GPUMesh), meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(daxa_u32), ent_to_mesh_inst_offsets_offsets)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(IndirectMemsetBufferCommand), clear_arena_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, U32ArenaBufferRef, bitfield_arena)
DAXA_DECL_TASK_HEAD_END

struct AllocMeshletInstBitfieldsPush {
    DAXA_TH_BLOB(AllocMeshletInstBitfieldsH, uses)
};

// - Goes over all visible meshlets from last frame again
// - Sets bits for all previously visible meshlets
// - prepopulates meshlet instances with previously visible meshlets
DAXA_DECL_TASK_HEAD_BEGIN(WriteFirstPassMeshletsAndBitfieldsH)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE_CONCURRENT, daxa_BufferPtr(RenderGlobalData), globals)
DAXA_TH_BUFFER(COMPUTE_SHADER_READ, command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(GPUMaterial), materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(VisibleMeshletList), visible_meshlets_prev)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletInstancesBufferHead), meshlet_instances_prev)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(daxa_u32), ent_to_mesh_inst_offsets_offsets)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, daxa_RWBufferPtr(MeshletInstancesBufferHead), meshlet_instances)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE, U32ArenaBufferRef, bitfield_arena)
DAXA_DECL_TASK_HEAD_END

struct WriteFirstPassMeshletsAndBitfieldsPush {
    DAXA_TH_BLOB(WriteFirstPassMeshletsAndBitfieldsH, uses)
};

#if __cplusplus

#include <daxa/utils/task_graph.hpp>

struct PrepopInfo {
    daxa::TaskGraph &task_graph;
    daxa::TaskBufferView meshes = {};
    daxa::TaskBufferView materials = {};
    daxa::TaskBufferView entity_mesh_groups = {};
    daxa::TaskBufferView mesh_group_manifest = {};
    daxa::TaskBufferView visible_meshlets_prev = {};
    daxa::TaskBufferView meshlet_instances_last_frame = {};
    daxa::TaskBufferView meshlet_instances = {};
    daxa::TaskBufferView &first_pass_meshlets_bitfield_offsets;
    daxa::TaskBufferView &first_pass_meshlets_bitfield_arena;
};
void task_prepopulate_meshlet_instances(PrepopInfo info);

#endif