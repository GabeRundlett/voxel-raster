#pragma once

#include <voxels/voxel_mesh.inl>
#include <voxels/pack_unpack.inl>

uint load_bit(daxa_BufferPtr(GpuInput) gpu_input, daxa_BufferPtr(VoxelBrickBitmask) bitmask, daxa_BufferPtr(ivec4) position, uint xi, uint yi, uint zi) {
    // ivec3 pos = deref(position).xyz;
    // float t = deref(gpu_input).time * 3 + float(uint((uint64_t(position) / 16)) & 0xffff) * 4;
    // vec3 offset = vec3(sin(t), cos(t), sin(t * 0.71)) * 1.0;
    // ivec3 p = pos * int(VOXEL_BRICK_SIZE) + ivec3(xi, yi, zi);
    // float r = cos(t * 2.95) * 0.5 + 0.5 + 2.0;
    // uint x = uint(length(vec3(p & 7) - 4.0 + 0.5 - offset) - r < 0);
    uint bit_index = xi + yi * VOXEL_BRICK_SIZE + zi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    uint word_index = bit_index / 32;
    uint in_word_index = bit_index % 32;
    return (deref(bitmask).bits[word_index] >> in_word_index) & 1; // | x;
}

uvec2 load_brick_faces_exposed(daxa_BufferPtr(GpuInput) gpu_input, daxa_BufferPtr(VoxelBrickBitmask) bitmask, uint xi, uint yi, uint axis) {
    // return uvec2(1);

    // uint brick_bitmask_metadata = deref(bitmask).metadata;
    // uint b_exposed = (brick_bitmask_metadata >> (axis + 0)) & 1;
    // uint t_exposed = (brick_bitmask_metadata >> (axis + 3)) & 1;
    // return uvec2(1 - b_exposed, 1 - t_exposed);

    uvec2 result = uvec2(0);
    {
        uint bit_index = xi + yi * VOXEL_BRICK_SIZE + (axis + 0) * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
        uint word_index = bit_index / 32;
        uint in_word_index = bit_index % 32;
        result.x = (deref(bitmask).neighbor_bits[word_index] >> in_word_index) & 1;
    }
    {
        uint bit_index = xi + yi * VOXEL_BRICK_SIZE + (axis + 3) * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
        uint word_index = bit_index / 32;
        uint in_word_index = bit_index % 32;
        result.y = (deref(bitmask).neighbor_bits[word_index] >> in_word_index) & 1;
    }
    return result;
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

vec2 brick_extent_pixels(daxa_BufferPtr(GpuInput) gpu_input, daxa_BufferPtr(VoxelChunk) voxel_chunks, BrickInstance brick_instance) {
    vec3 ndc_min;
    vec3 ndc_max;

    VoxelChunk voxel_chunk = deref(advance(voxel_chunks, brick_instance.chunk_index));
    ivec4 pos_scl = deref(advance(voxel_chunk.pos_scl, brick_instance.brick_index));

    vec3 p0 = ivec3(voxel_chunk.pos * VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(0);
    vec3 p1 = ivec3(voxel_chunk.pos * VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(VOXEL_BRICK_SIZE);
    int scl = pos_scl.w + 8;
    const float SCL = (float(1 << scl) / float(1 << 8));
    p0 *= SCL;
    p1 *= SCL;

    vec3 vertices[8] = vec3[8](
        vec3(p0.x, p0.y, p0.z),
        vec3(p1.x, p0.y, p0.z),
        vec3(p0.x, p1.y, p0.z),
        vec3(p1.x, p1.y, p0.z),
        vec3(p0.x, p0.y, p1.z),
        vec3(p1.x, p0.y, p1.z),
        vec3(p0.x, p1.y, p1.z),
        vec3(p1.x, p1.y, p1.z));

    [[unroll]] for (uint vert_i = 0; vert_i < 8; ++vert_i) {
        vec4 vs_h = deref(gpu_input).cam.world_to_view * vec4(vertices[vert_i], 1);
        vec4 cs_h = deref(gpu_input).cam.view_to_sample * vs_h;
        vec3 p = cs_h.xyz / cs_h.w;
        if (vert_i == 0) {
            ndc_min = p;
            ndc_max = p;
        } else {
            ndc_min = min(ndc_min, p);
            ndc_max = max(ndc_max, p);
        }
    }

    vec2 ndc_extent = ndc_max.xy - ndc_min.xy;
    vec2 pixel_extent = ndc_extent * 0.5 * deref(gpu_input).render_size;

    return pixel_extent;
}

Voxel load_voxel(daxa_BufferPtr(GpuInput) gpu_input, daxa_BufferPtr(VoxelChunk) voxel_chunk, uint brick_index, uvec3 voxel_pos) {
    uint voxel_index = voxel_pos.x + voxel_pos.y * VOXEL_BRICK_SIZE + voxel_pos.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    return unpack_voxel(deref(advance(deref(voxel_chunk).attribs, brick_index)).packed_voxels[voxel_index]);

    // uint word_index = voxel_index / 32;
    // uint in_word_index = voxel_index % 32;
    // uint bit = (deref(advance(deref(voxel_chunk).bitmasks, brick_index)).bits[word_index] >> in_word_index) & 1;
    // if (bit != 0) {
    //     return unpack_voxel(deref(advance(deref(voxel_chunk).attribs, brick_index)).packed_voxels[voxel_index]);
    // } else {
    //     daxa_BufferPtr(ivec4) position = daxa_BufferPtr(ivec4)(advance(deref(voxel_chunk).pos_scl, brick_index));
    //     ivec3 pos = deref(position).xyz;
    //     float t = deref(gpu_input).time * 3 + float(uint((uint64_t(position) / 16)) & 0xffff) * 4;
    //     vec3 offset = vec3(sin(t), cos(t), sin(t * 0.71)) * 1.0;
    //     ivec3 p = pos * int(VOXEL_BRICK_SIZE) + ivec3(voxel_pos);
    //     Voxel result;
    //     result.nrm = normalize(vec3(p & 7) - 4.0 + 0.5 - offset);
    //     result.col = vec3(223, 152, 7) / 255;
    //     return result;
    // }
}
