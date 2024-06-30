#include "voxel_rasterizer.inl"
#include "allocators.glsl"

DAXA_DECL_PUSH_CONSTANT(ClearDrawFlagsPush, push)

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
void main() {
    uint brick_instance_index = gl_GlobalInvocationID.x + 1;
    if (!is_valid_index(daxa_BufferPtr(BrickInstance)(as_address(push.uses.brick_instance_allocator)), brick_instance_index)) {
        return;
    }

    BrickInstance brick_instance = deref(advance(push.uses.brick_instance_allocator, brick_instance_index));
    VoxelChunk voxel_chunk = deref(advance(push.uses.chunks, brick_instance.chunk_index));
    deref(advance(voxel_chunk.flags, brick_instance.brick_index)) = 0;
}
