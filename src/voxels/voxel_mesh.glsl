#pragma once

#include <voxels/voxel_mesh.inl>

uint load_bit(daxa_BufferPtr(VoxelBrickBitmask) bitmask, uint xi, uint yi, uint zi) {
    uint bit_index = xi + yi * VOXEL_BRICK_SIZE + zi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    uint word_index = bit_index / 32;
    uint in_word_index = bit_index % 32;
    return (deref(bitmask).bits[word_index] >> in_word_index) & 1;
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

uint load_sequential_face_bits(
    daxa_BufferPtr(VoxelBrickFaceBitmask) face_bitmask,
    daxa_BufferPtr(VoxelBrickBitmask) bitmask,
    uint strip_i) {
    if (strip_i < 2) {
        return 0xffffffff;
    }
    strip_i -= 2;
    if (strip_i < (VOXELS_PER_BRICK * 3 / 32)) {
        return deref(face_bitmask).bits[strip_i];
    }
    return 0xffffffff;
}

uint sequential_face_bit_voxel_id(uint strip_i, uint in_strip_i) {
    uint index = in_strip_i + strip_i * 32;
    return index;
}
