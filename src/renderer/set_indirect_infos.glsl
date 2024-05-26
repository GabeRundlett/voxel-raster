#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(SetIndirectInfos0Push, push)

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
    deref(push.uses.indirect_infos[0]).x = deref(push.uses.brick_instance_allocator).instance_count;
    deref(push.uses.indirect_infos[0]).y = 1;
    deref(push.uses.indirect_infos[0]).z = 1;

    deref(push.uses.indirect_infos[1]).x = deref(push.uses.brick_instance_allocator).instance_count;
    deref(push.uses.indirect_infos[1]).y = 1;
    deref(push.uses.indirect_infos[1]).z = 1;
}
