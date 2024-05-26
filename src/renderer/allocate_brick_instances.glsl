#include <shared.inl>
#include <renderer/meshlet_allocator.glsl>

DAXA_DECL_PUSH_CONSTANT(AllocateBrickInstancesPush, push)

shared uint instance_offset;

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main() {
    uint chunk_index = gl_WorkGroupID.x;
    // if (chunk_index >= deref(push.uses.gpu_input).chunk_n) {
    //     return;
    // }

    uint brick_n = deref(push.uses.chunks[chunk_index]).brick_n;

    if (gl_LocalInvocationIndex == 0) {
        instance_offset = allocate_brick_instances(push.uses.brick_instance_allocator, brick_n);
        // deref(push.uses.chunks[chunk_index]).instance_offset = instance_offset;
    }

    barrier();

    for (uint brick_index = gl_LocalInvocationIndex; brick_index < brick_n; brick_index += 64) {
        BrickInstance brick_instance;
        brick_instance.chunk_index = chunk_index;
        brick_instance.brick_index = brick_index;
        uint brick_instance_index = instance_offset + brick_index;
        deref(push.uses.brick_instance_allocator[brick_instance_index]) = brick_instance;
    }
}
