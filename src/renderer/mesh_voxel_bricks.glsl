#include <shared.inl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/allocators.glsl>

DAXA_DECL_PUSH_CONSTANT(MeshVoxelBricksPush, push)

shared VoxelBrickMesh result_mesh;

// TODO: How can I remove this?
shared uint face_ids[MAX_OUTER_FACES_PER_BRICK];
uint brick_instance_index;
BrickInstance brick_instance;
daxa_BufferPtr(VoxelChunk) chunk;

void init_results() {
    if (gl_LocalInvocationIndex == 0) {
        result_mesh.face_count = 0;
        result_mesh.meshlet_start = 0;
    }
    barrier();
}

void write_results() {
    barrier();
    uint meshlet_n = (result_mesh.face_count + (MAX_FACES_PER_MESHLET - 1)) / MAX_FACES_PER_MESHLET;

    if (gl_LocalInvocationIndex == 0) {
        // compute size
        float size_x = brick_extent_pixels(push.uses.gpu_input, push.uses.chunks, brick_instance).x / VOXEL_BRICK_SIZE;
        result_mesh.meshlet_start = allocate_meshlets(push.uses.meshlet_allocator, meshlet_n, size_x < 8);

        if (result_mesh.meshlet_start != 0) {
            deref(deref(chunk).meshes[brick_instance.brick_index]).face_count = result_mesh.face_count;
            deref(deref(chunk).meshes[brick_instance.brick_index]).meshlet_start = result_mesh.meshlet_start;
        }
    }

    barrier();

    if (gl_LocalInvocationIndex < meshlet_n && result_mesh.meshlet_start != 0) {
        for (uint i = 0; i < 32; ++i) {
            uint face_i = i + gl_LocalInvocationIndex * 32;
            uint face_id = 0xffffffff;
            if (face_i < result_mesh.face_count) {
                face_id = face_ids[face_i];
            }
            deref(push.uses.meshlet_allocator[result_mesh.meshlet_start + gl_LocalInvocationIndex]).faces[i] = PackedVoxelBrickFace(face_id);
        }
        deref(push.uses.meshlet_metadata[result_mesh.meshlet_start + gl_LocalInvocationIndex]).brick_instance_index = brick_instance_index;
    }
}

layout(local_size_x = VOXEL_BRICK_SIZE, local_size_y = VOXEL_BRICK_SIZE, local_size_z = 3) in;
void main() {
    brick_instance_index = gl_WorkGroupID.x + 1 + deref(push.uses.indirect_info).offset;

    if (!is_valid_index(daxa_BufferPtr(BrickInstance)(push.uses.brick_instance_allocator), brick_instance_index)) {
        return;
    }

    brick_instance = deref(push.uses.brick_instance_allocator[brick_instance_index]);
    chunk = push.uses.chunks[brick_instance.chunk_index];

    init_results();

    uint xi = gl_LocalInvocationID.x;
    uint yi = gl_LocalInvocationID.y;
    uint fi = gl_LocalInvocationID.z;

    uint bit_strip = load_strip(push.uses.gpu_input, daxa_BufferPtr(VoxelBrickBitmask)(deref(chunk).bitmasks[brick_instance.brick_index]), daxa_BufferPtr(ivec4)(deref(chunk).pos_scl[brick_instance.brick_index]), xi, yi, fi);
    uvec2 edges_exposed = load_brick_faces_exposed(push.uses.gpu_input, daxa_BufferPtr(VoxelBrickBitmask)(deref(chunk).bitmasks[brick_instance.brick_index]), xi, yi, fi);

    uint b_edge_mask = bit_strip & ~(bit_strip << 1);
    uint t_edge_mask = bit_strip & ~(bit_strip >> 1);

    b_edge_mask &= ~(edges_exposed[0]);
    t_edge_mask &= ~((edges_exposed[1]) << (VOXEL_BRICK_SIZE - 1));

    uint strip_index = face_bitmask_strip_index(xi, yi, fi);
    uint bit_index = strip_index * VOXEL_BRICK_SIZE;
    uint word_index = bit_index / 32;

    uint in_word_index = bit_index % 32;
    uint b_edge_count = bitCount(b_edge_mask);
    uint t_edge_count = bitCount(t_edge_mask);

    uint face_offset = atomicAdd(result_mesh.face_count, b_edge_count + t_edge_count);

    uint shift = 0;
    for (uint i = 0; i < b_edge_count; ++i) {
        uint lsb = findLSB(b_edge_mask >> shift);
        shift += lsb + 1;
        uint in_strip_index = shift - 1;
        face_ids[face_offset + i] = pack(VoxelBrickFace(unswizzle_from_strip_coord(uvec3(xi, yi, in_strip_index), fi), fi * 2)).data;
    }

    shift = 0;
    for (uint i = 0; i < t_edge_count; ++i) {
        uint lsb = findLSB(t_edge_mask >> shift);
        shift += lsb + 1;
        uint in_strip_index = shift - 1;
        face_ids[face_offset + i + b_edge_count] = pack(VoxelBrickFace(unswizzle_from_strip_coord(uvec3(xi, yi, in_strip_index), fi), fi * 2 + 1)).data;
    }

    write_results();
}
