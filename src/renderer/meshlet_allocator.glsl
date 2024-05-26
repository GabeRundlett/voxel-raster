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
// bool is_valid_index(daxa_BufferPtr(VoxelMeshlet) meshlet_allocator, uint meshlet_i) {
//     daxa_BufferPtr(VoxelMeshletAllocatorState) allocator_state = daxa_BufferPtr(VoxelMeshletAllocatorState)(meshlet_allocator);
//     return meshlet_i < deref(allocator_state).meshlet_count && meshlet_i > 0;
// }

uint allocate_brick_instances(daxa_RWBufferPtr(BrickInstance) brick_instance_allocator, uint brick_instance_n) {
    daxa_RWBufferPtr(BrickInstanceAllocatorState) allocator_state = daxa_RWBufferPtr(BrickInstanceAllocatorState)(brick_instance_allocator);
    uint result = atomicAdd(deref(allocator_state).instance_count, brick_instance_n);
    if (result >= MAX_BRICK_INSTANCE_COUNT) {
        return 0;
    }
    return result + 1;
}
bool is_valid_index(daxa_BufferPtr(BrickInstance) brick_instance_allocator, uint brick_instance_i) {
    daxa_BufferPtr(BrickInstanceAllocatorState) allocator_state = daxa_BufferPtr(BrickInstanceAllocatorState)(brick_instance_allocator);
    return brick_instance_i <= deref(allocator_state).instance_count && brick_instance_i > 0;
}
