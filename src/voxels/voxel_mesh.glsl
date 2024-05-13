#pragma once

#include <voxels/voxel_mesh.inl>

uint load_bit(daxa_BufferPtr(VoxelBrickBitmask) bitmask, uint xi, uint yi, uint zi) {
    uint bit_index = xi + yi * VOXEL_BRICK_SIZE + zi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    uint word_index = bit_index / 32;
    uint in_word_index = bit_index % 32;
    return (deref(bitmask).bits[word_index] >> in_word_index) & 1;
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

uint load_strip(daxa_BufferPtr(VoxelBrickBitmask) bitmask, uint xi, uint yi, uint axis) {
    uint result = 0;
    switch (axis) {
    case 0: { // x-axis (yz plane)
        for (uint i = 0; i < VOXEL_BRICK_SIZE; ++i) {
            result |= load_bit(bitmask, i, xi, yi) << i;
        }
    } break;
    case 1: { // y-axis (xz plane)
        for (uint i = 0; i < VOXEL_BRICK_SIZE; ++i) {
            result |= load_bit(bitmask, xi, i, yi) << i;
        }
    } break;
    case 2: { // z-axis (xy plane)
        for (uint i = 0; i < VOXEL_BRICK_SIZE; ++i) {
            result |= load_bit(bitmask, xi, yi, i) << i;
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
