#pragma once

struct VisbufferPayload {
    uint voxel_id;
    uint brick_id;
};
struct PackedVisbufferPayload {
    uint data;
};

PackedVisbufferPayload pack(VisbufferPayload payload) {
    PackedVisbufferPayload result;
    result.data = payload.voxel_id & 0x1ff;
    result.data |= (payload.brick_id & 0xfffff) << 9;
    result.data += 1;
    return result;
}
VisbufferPayload unpack(PackedVisbufferPayload packed_payload) {
    VisbufferPayload result;
    packed_payload.data -= 1;
    result.voxel_id = packed_payload.data & 0x1ff;
    result.brick_id = (packed_payload.data >> 9) & 0xfffff;
    return result;
}
