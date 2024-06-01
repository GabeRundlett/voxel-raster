#pragma once

#include <voxels/voxel_mesh.inl>

uint load_bit(daxa_BufferPtr(GpuInput) gpu_input, daxa_BufferPtr(VoxelBrickBitmask) bitmask, daxa_BufferPtr(ivec4) position, uint xi, uint yi, uint zi) {
    // ivec3 pos = deref(position).xyz;
    // uint a = uint(uint64_t(bitmask));
    // uint b = uint(uint64_t(bitmask) >> uint64_t(32));
    // float speed = 4.0; // + hash11(float(a ^ b)) * 100;
    // float offset = 0.0; // + hash11(float(a ^ b)) * 100;
    // ivec3 p = pos * int(VOXEL_BRICK_SIZE) + ivec3(xi, yi, zi);
    // p.x = p.x % 16;
    // uint x = uint(abs(p.x + sin(deref(gpu_input).time * speed + p.y * 0.5 + offset) * (VOXEL_BRICK_SIZE / 2 - 1) - VOXEL_BRICK_SIZE / 2) < 2);
    uint bit_index = xi + yi * VOXEL_BRICK_SIZE + zi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    uint word_index = bit_index / 32;
    uint in_word_index = bit_index % 32;
    return (deref(bitmask).bits[word_index] >> in_word_index) & 1; // | x;
}

uvec2 load_brick_faces_exposed(daxa_BufferPtr(GpuInput) gpu_input, daxa_BufferPtr(VoxelBrickBitmask) bitmask, uint xi, uint yi, uint axis) {
    // return uvec2(1);
    uint brick_bitmask_metadata = deref(bitmask).metadata;
    uint b_exposed = (brick_bitmask_metadata >> (axis + 0)) & 1;
    uint t_exposed = (brick_bitmask_metadata >> (axis + 3)) & 1;
    return uvec2(b_exposed, t_exposed);
}

uvec3 unswizzle_from_strip_coord(uvec3 pos, uint axis) {
    switch (axis) {
    case 0: { // x-axis (yz plane)
        return pos.zxy;
    } break;
    case 1: { // y-axis (xz plane)
        return pos.xzy;
    } break;
    }
    return pos;
}

uint load_strip(daxa_BufferPtr(GpuInput) gpu_input, daxa_BufferPtr(VoxelBrickBitmask) bitmask, daxa_BufferPtr(ivec4) position, uint xi, uint yi, uint axis) {
    uint result = 0;
    switch (axis) {
    case 0: { // x-axis (yz plane)
        for (uint i = 0; i < VOXEL_BRICK_SIZE; ++i) {
            result |= load_bit(gpu_input, bitmask, position, i, xi, yi) << i;
        }
    } break;
    case 1: { // y-axis (xz plane)
        for (uint i = 0; i < VOXEL_BRICK_SIZE; ++i) {
            result |= load_bit(gpu_input, bitmask, position, xi, i, yi) << i;
        }
    } break;
    case 2: { // z-axis (xy plane)
        for (uint i = 0; i < VOXEL_BRICK_SIZE; ++i) {
            result |= load_bit(gpu_input, bitmask, position, xi, yi, i) << i;
        }
    } break;
    }
    return result;
}

uint face_bitmask_strip_index(uint strip_xi, uint strip_yi, uint strip_fi) {
    return strip_xi + strip_yi * VOXEL_BRICK_SIZE + strip_fi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
}

PackedVoxelBrickFace pack(VoxelBrickFace face) {
    PackedVoxelBrickFace result = PackedVoxelBrickFace(0);
    result.data += face.pos.x;
    result.data += face.pos.y * VOXEL_BRICK_SIZE;
    result.data += face.pos.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    result.data += face.axis * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    return result;
}
VoxelBrickFace unpack(PackedVoxelBrickFace packed_face) {
    VoxelBrickFace result;
    uint face_id = packed_face.data;
    result.pos.x = face_id % VOXEL_BRICK_SIZE;
    result.pos.y = (face_id / VOXEL_BRICK_SIZE) % VOXEL_BRICK_SIZE;
    result.pos.z = (face_id / VOXEL_BRICK_SIZE / VOXEL_BRICK_SIZE) % VOXEL_BRICK_SIZE;
    result.axis = face_id / VOXEL_BRICK_SIZE / VOXEL_BRICK_SIZE / VOXEL_BRICK_SIZE;
    return result;
}
