#include <shared.inl>
#include <voxels/voxel_mesh.glsl>

DAXA_DECL_PUSH_CONSTANT(ResetMeshletAllocatorPush, push)

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
    deref(push.uses.meshlet_allocator_state).meshlet_count = 0;
}
