#include <shared.inl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/meshlet_allocator.glsl>

DAXA_DECL_PUSH_CONSTANT(MeshVoxelBricksPush, push)

shared VoxelBrickMesh result_mesh;

// TODO: How can I remove this?
shared uint face_ids[MAX_OUTER_FACES_PER_BRICK];

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
        result_mesh.meshlet_start = allocate_meshlets(push.uses.meshlet_allocator, meshlet_n);

        deref(push.uses.meshes[gl_WorkGroupID.x]).face_count = result_mesh.face_count;
        deref(push.uses.meshes[gl_WorkGroupID.x]).meshlet_start = result_mesh.meshlet_start;
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
        deref(push.uses.meshlet_metadata[result_mesh.meshlet_start + gl_LocalInvocationIndex]).brick_id = gl_WorkGroupID.x;
    }
}

layout(local_size_x = VOXEL_BRICK_SIZE, local_size_y = VOXEL_BRICK_SIZE, local_size_z = 3) in;
void main() {
    init_results();

    uint xi = gl_LocalInvocationID.x;
    uint yi = gl_LocalInvocationID.y;
    uint fi = gl_LocalInvocationID.z;

    uint bit_strip = load_strip(push.uses.bitmasks[gl_WorkGroupID.x], xi, yi, fi);

    uint b_edge_mask = bit_strip & ~(bit_strip << 1);
    uint t_edge_mask = bit_strip & ~(bit_strip >> 1);
    uint edge_mask = b_edge_mask | t_edge_mask;

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
