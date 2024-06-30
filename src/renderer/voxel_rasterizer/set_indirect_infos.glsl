#include "voxel_rasterizer.inl"

DAXA_DECL_PUSH_CONSTANT(SetIndirectInfosPush, push)

#if !defined(SET_TYPE)
#define SET_TYPE 0
#endif

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
#if SET_TYPE == 1
    uint prev_instance_count = deref(advance(push.uses.indirect_info, 0)).x;
    uint prev_instance_offset = deref(advance(push.uses.indirect_info, 0)).offset;
    uint new_instance_count = deref(push.uses.brick_instance_allocator).instance_count;

    deref(advance(push.uses.indirect_info, 0)).x = new_instance_count - (prev_instance_count + prev_instance_offset);
    deref(advance(push.uses.indirect_info, 0)).y = 1;
    deref(advance(push.uses.indirect_info, 0)).z = 1;
    deref(advance(push.uses.indirect_info, 0)).offset = prev_instance_count + prev_instance_offset;
#elif SET_TYPE == 2
    uint prev_sw_meshlet_count = deref(advance(push.uses.indirect_info, 2)).x;
    uint prev_sw_meshlet_offset = deref(advance(push.uses.indirect_info, 2)).offset;
    uint prev_hw_meshlet_count = deref(advance(push.uses.indirect_info, 3)).x;
    uint prev_hw_meshlet_offset = deref(advance(push.uses.indirect_info, 3)).offset;
    uint new_sw_meshlet_count = deref(push.uses.meshlet_allocator).sw_meshlet_count;
    uint new_hw_meshlet_count = deref(push.uses.meshlet_allocator).hw_meshlet_count;

    // helper counters
    deref(advance(push.uses.indirect_info, 2)).x = new_sw_meshlet_count - (prev_sw_meshlet_offset + prev_sw_meshlet_count);
    deref(advance(push.uses.indirect_info, 2)).y = 1;
    deref(advance(push.uses.indirect_info, 2)).z = 1;
    deref(advance(push.uses.indirect_info, 2)).offset = prev_sw_meshlet_offset + prev_sw_meshlet_count;

    // compute raster info
    deref(advance(push.uses.indirect_info, 1)).x = (new_sw_meshlet_count - (prev_sw_meshlet_offset + prev_sw_meshlet_count) + 3) / 4;
    deref(advance(push.uses.indirect_info, 1)).y = 1;
    deref(advance(push.uses.indirect_info, 1)).z = 1;
    deref(advance(push.uses.indirect_info, 1)).offset = prev_sw_meshlet_offset + prev_sw_meshlet_count;

    // hardware raster info
    deref(advance(push.uses.indirect_info, 3)).x = new_hw_meshlet_count - (prev_hw_meshlet_offset + prev_hw_meshlet_count);
    deref(advance(push.uses.indirect_info, 3)).y = 1;
    deref(advance(push.uses.indirect_info, 3)).z = 1;
    deref(advance(push.uses.indirect_info, 3)).offset = prev_hw_meshlet_offset + prev_hw_meshlet_count;
#else
    uint new_instance_count = deref(push.uses.brick_instance_allocator).instance_count;
    uint new_sw_meshlet_count = deref(push.uses.meshlet_allocator).sw_meshlet_count;
    uint new_hw_meshlet_count = deref(push.uses.meshlet_allocator).hw_meshlet_count;

    deref(advance(push.uses.indirect_info, 0)).x = new_instance_count;
    deref(advance(push.uses.indirect_info, 0)).y = 1;
    deref(advance(push.uses.indirect_info, 0)).z = 1;
    deref(advance(push.uses.indirect_info, 0)).offset = 0;

    deref(advance(push.uses.indirect_info, 2)).x = new_sw_meshlet_count;
    deref(advance(push.uses.indirect_info, 2)).y = 1;
    deref(advance(push.uses.indirect_info, 2)).z = 1;
    deref(advance(push.uses.indirect_info, 2)).offset = 0;

    deref(advance(push.uses.indirect_info, 3)).x = new_hw_meshlet_count;
    deref(advance(push.uses.indirect_info, 3)).y = 1;
    deref(advance(push.uses.indirect_info, 3)).z = 1;
    deref(advance(push.uses.indirect_info, 3)).offset = 0;

    deref(advance(push.uses.indirect_info, 1)).x = (new_sw_meshlet_count + 3) / 4;
    deref(advance(push.uses.indirect_info, 1)).y = 1;
    deref(advance(push.uses.indirect_info, 1)).z = 1;
    deref(advance(push.uses.indirect_info, 1)).offset = 0;
#endif
}
