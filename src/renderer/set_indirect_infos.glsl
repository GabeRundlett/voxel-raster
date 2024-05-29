#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(SetIndirectInfosPush, push)

#if !defined(SET_TYPE)
#define SET_TYPE 0
#endif

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
    uint prev_instance_count = deref(push.uses.indirect_info[0]).x;
    uint prev_instance_offset = deref(push.uses.indirect_info[0]).offset;
    uint new_instance_count = deref(push.uses.brick_instance_allocator).instance_count;

#if SET_TYPE == 1
    deref(push.uses.indirect_info[0]).x = new_instance_count - (prev_instance_count + prev_instance_offset);
    deref(push.uses.indirect_info[0]).y = 1;
    deref(push.uses.indirect_info[0]).z = 1;
    deref(push.uses.indirect_info[0]).offset = prev_instance_count + prev_instance_offset;
#else
    deref(push.uses.indirect_info[0]).x = new_instance_count;
    deref(push.uses.indirect_info[0]).y = 1;
    deref(push.uses.indirect_info[0]).z = 1;
    deref(push.uses.indirect_info[0]).offset = 0;
#endif
}
