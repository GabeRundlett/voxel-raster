#include <shared.inl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/meshlet_allocator.glsl>

DAXA_DECL_PUSH_CONSTANT(MeshVoxelBricksPush, push)

// shared VoxelBrickFaceBitmask result_face_bitmask;
shared VoxelBrickMesh result_mesh;

// TODO: Remove this?
shared uint face_ids[MAX_OUTER_FACES_PER_BRICK];

void init_results() {
    // if (gl_LocalInvocationIndex < VOXELS_PER_BRICK * 3 / 32) {
    //     result_face_bitmask.bits[gl_LocalInvocationIndex] = 0;
    // }
    if (gl_LocalInvocationIndex == 0) {
        result_mesh.face_count = 0;
        result_mesh.meshlet_start = 0;
    }
    barrier();
}

void write_results() {
    barrier();
    // if (gl_LocalInvocationIndex < VOXELS_PER_BRICK * 3 / 32) {
    //     deref(push.uses.face_bitmasks[gl_WorkGroupID.x]).bits[gl_LocalInvocationIndex] = result_face_bitmask.bits[gl_LocalInvocationIndex];
    // }
    uint meshlet_n = (result_mesh.face_count + (MAX_FACES_PER_MESHLET - 1)) / MAX_FACES_PER_MESHLET;

    if (gl_LocalInvocationIndex == 0) {
        result_mesh.meshlet_start = allocate_meshlets(push.uses.meshlet_allocator, meshlet_n);

        deref(push.uses.meshes[gl_WorkGroupID.x]).face_count = result_mesh.face_count;
        deref(push.uses.meshes[gl_WorkGroupID.x]).meshlet_start = result_mesh.meshlet_start;
    }

    barrier();

    if (gl_LocalInvocationIndex < meshlet_n) {
        for (uint i = 0; i < 32; ++i) {
            uint face_i = i + gl_LocalInvocationIndex * 32;
            uint face_id = 0xffffffff;
            if (face_i < result_mesh.face_count) {
                face_id = face_ids[face_i];
            }
            deref(push.uses.meshlet_allocator[result_mesh.meshlet_start + gl_LocalInvocationIndex]).faces[i] = face_id;
        }
    }
}

layout(local_size_x = VOXEL_BRICK_SIZE, local_size_y = VOXEL_BRICK_SIZE, local_size_z = 3) in;
void main() {
    init_results();

    uint xi = gl_LocalInvocationID.x;
    uint yi = gl_LocalInvocationID.y;
    uint fi = gl_LocalInvocationID.z;

    uint bit_strip = load_strip(push.uses.bitmasks[gl_WorkGroupID.x], xi, yi, fi);
    uint edge_mask = bit_strip & ~(bit_strip << 1);

    uint strip_index = face_bitmask_strip_index(xi, yi, fi);
    uint bit_index = strip_index * VOXEL_BRICK_SIZE;
    uint word_index = bit_index / 32;

    uint in_word_index = bit_index % 32;
    // atomicOr(result_face_bitmask.bits[word_index], edge_mask << in_word_index);
    uint edge_count = bitCount(edge_mask);

    uint face_offset = atomicAdd(result_mesh.face_count, edge_count);

    uint shift = 0;
    for (uint i = 0; i < edge_count; ++i) {
        shift += findLSB(edge_mask) + 1;
        uint in_strip_index = shift;
        face_ids[face_offset + i] = strip_index + in_strip_index * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * 3;
    }

    if ((bit_strip & 1) != 0) {
        face_offset = atomicAdd(result_mesh.face_count, 1);
        uint in_strip_index = 0;
        face_ids[face_offset] = strip_index + in_strip_index * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * 3;
    }

    write_results();
}
