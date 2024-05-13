#pragma once

#include <voxels/voxel_mesh.inl>

uint allocate_meshlets(daxa_RWBufferPtr(VoxelMeshlet) meshlet_allocator, uint meshlet_n) {
    daxa_RWBufferPtr(VoxelMeshletAllocatorState) allocator_state = daxa_RWBufferPtr(VoxelMeshletAllocatorState)(meshlet_allocator);
    uint result = atomicAdd(deref(allocator_state).meshlet_count, meshlet_n);
    if (result >= MAX_MESHLET_COUNT) {
        return 0;
    }
    return result + 1;
}
