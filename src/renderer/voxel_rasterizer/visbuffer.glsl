#pragma once

struct VisbufferPayload {
    uint face_id;
    uint meshlet_id;
};
struct PackedVisbufferPayload {
    uint data;
};

PackedVisbufferPayload pack(VisbufferPayload payload) {
    PackedVisbufferPayload result;
    result.data = payload.face_id & 0x1ff;
    result.data |= payload.meshlet_id << 9;
    result.data += 1;
    return result;
}
VisbufferPayload unpack(PackedVisbufferPayload packed_payload) {
    VisbufferPayload result;
    packed_payload.data -= 1;
    result.face_id = packed_payload.data & 0x1ff;
    result.meshlet_id = packed_payload.data >> 9;
    return result;
}
