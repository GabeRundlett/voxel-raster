#pragma once

#include <voxels/voxel_mesh.inl>

uint allocate_meshlets(daxa_RWBufferPtr(VoxelMeshlet) meshlet_allocator, uint meshlet_n, bool compute_raster) {
    daxa_RWBufferPtr(VoxelMeshletAllocatorState) allocator_state = daxa_RWBufferPtr(VoxelMeshletAllocatorState)(meshlet_allocator);
    if (compute_raster) {
        uint result = atomicAdd(deref(allocator_state).sw_meshlet_count, meshlet_n);
        if (result <= MAX_SW_MESHLET_COUNT - meshlet_n) {
            return result + 1;
        }
    }
    uint result = atomicAdd(deref(allocator_state).hw_meshlet_count, meshlet_n);
    if (result > MAX_MESHLET_COUNT - MAX_SW_MESHLET_COUNT - meshlet_n) {
        return 0;
    }
    return result + MAX_SW_MESHLET_COUNT + 1;
}
bool is_valid_meshlet_index(daxa_BufferPtr(VoxelMeshlet) meshlet_allocator, uint meshlet_i) {
    daxa_BufferPtr(VoxelMeshletAllocatorState) allocator_state = daxa_BufferPtr(VoxelMeshletAllocatorState)(meshlet_allocator);
    if (meshlet_i - 1 < MAX_SW_MESHLET_COUNT) {
        return meshlet_i - 1 < deref(allocator_state).sw_meshlet_count;
    }
    return meshlet_i - 1 < min(deref(allocator_state).hw_meshlet_count + MAX_SW_MESHLET_COUNT, MAX_MESHLET_COUNT);
}

uint allocate_brick_instances(daxa_RWBufferPtr(BrickInstance) brick_instance_allocator, uint brick_instance_n) {
    daxa_RWBufferPtr(BrickInstanceAllocatorState) allocator_state = daxa_RWBufferPtr(BrickInstanceAllocatorState)(brick_instance_allocator);
    uint result = atomicAdd(deref(allocator_state).instance_count, brick_instance_n);
    if (result > MAX_BRICK_INSTANCE_COUNT - 1) {
        return 0;
    }
    return result + 1;
}
bool is_valid_index(daxa_BufferPtr(BrickInstance) brick_instance_allocator, uint brick_instance_i) {
    daxa_BufferPtr(BrickInstanceAllocatorState) allocator_state = daxa_BufferPtr(BrickInstanceAllocatorState)(brick_instance_allocator);
    return brick_instance_i - 1 < min(deref(allocator_state).instance_count, MAX_BRICK_INSTANCE_COUNT);
}
