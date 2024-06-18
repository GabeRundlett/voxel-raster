#include <shared.inl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/allocators.glsl>

DAXA_DECL_PUSH_CONSTANT(MeshVoxelBricksPush, push)

shared VoxelBrickMesh result_mesh;
// TODO: How can I remove this?
shared uint face_ids[MAX_OUTER_FACES_PER_BRICK * 9 / 16];

layout(local_size_x = VOXEL_BRICK_SIZE, local_size_y = VOXEL_BRICK_SIZE, local_size_z = 3) in;
void main() {
    uint brick_instance_index = gl_WorkGroupID.x + 1 + deref(push.uses.indirect_info).offset;

    if (!is_valid_index(daxa_BufferPtr(BrickInstance)(push.uses.brick_instance_allocator), brick_instance_index)) {
        return;
    }

    BrickInstance brick_instance = deref(push.uses.brick_instance_allocator[brick_instance_index]);
    daxa_BufferPtr(VoxelChunk) chunk = push.uses.chunks[brick_instance.chunk_index];
    ivec4 pos_scl = deref(deref(chunk).pos_scl[brick_instance.brick_index]);

    vec3 p0 = ivec3(deref(chunk).pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(0);
    vec3 p1 = ivec3(deref(chunk).pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(VOXEL_BRICK_SIZE);
    int scl = pos_scl.w + 8;
#define SCL (float(1 << scl) / float(1 << 8))
    p0 *= SCL;
    p1 *= SCL;

    vec3 cam_pos = vec3(
        deref(push.uses.gpu_input).cam.view_to_world[3][0],
        deref(push.uses.gpu_input).cam.view_to_world[3][1],
        deref(push.uses.gpu_input).cam.view_to_world[3][2]);
    p0 -= cam_pos;
    p1 -= cam_pos;

    uint xi = gl_LocalInvocationID.x;
    uint yi = gl_LocalInvocationID.y;
    uint fi = gl_LocalInvocationID.z;

    uint bit_strip = load_strip(push.uses.gpu_input, daxa_BufferPtr(VoxelBrickBitmask)(deref(chunk).bitmasks[brick_instance.brick_index]), daxa_BufferPtr(ivec4)(deref(chunk).pos_scl[brick_instance.brick_index]), xi, yi, fi);
    uvec2 edges_exposed = load_brick_faces_exposed(push.uses.gpu_input, daxa_BufferPtr(VoxelBrickBitmask)(deref(chunk).bitmasks[brick_instance.brick_index]), xi, yi, fi);

    uint b_edge_mask = bit_strip & ~(bit_strip << 1);
    uint t_edge_mask = bit_strip & ~(bit_strip >> 1);

    b_edge_mask &= ~(edges_exposed[0]);
    t_edge_mask &= ~(edges_exposed[1] << (VOXEL_BRICK_SIZE - 1));

    uint strip_index = face_bitmask_strip_index(xi, yi, fi);
    uint bit_index = strip_index * VOXEL_BRICK_SIZE;
    uint word_index = bit_index / 32;

    if (fi == 0) {
        if (p1.x < 0) {
            b_edge_mask = 0x0;
        } else if (p0.x > 0) {
            t_edge_mask = 0x0;
        } else {
            int cam_voxel_pos = int(cam_pos.x * 16.0) % int(VOXEL_BRICK_SIZE);
            int mask = (1 << cam_voxel_pos) - 1;
            b_edge_mask &= ~mask;
            t_edge_mask &= mask;
        }
    }
    if (fi == 1) {
        if (p1.y < 0) {
            b_edge_mask = 0x0;
        } else if (p0.y > 0) {
            t_edge_mask = 0x0;
        } else {
            int cam_voxel_pos = int(cam_pos.y * 16.0) % int(VOXEL_BRICK_SIZE);
            int mask = (1 << cam_voxel_pos) - 1;
            b_edge_mask &= ~mask;
            t_edge_mask &= mask;
        }
    }
    if (fi == 2) {
        if (p1.z < 0) {
            b_edge_mask = 0x0;
        } else if (p0.z > 0) {
            t_edge_mask = 0x0;
        } else {
            int cam_voxel_pos = int(cam_pos.z * 16.0) % int(VOXEL_BRICK_SIZE);
            int mask = (1 << cam_voxel_pos) - 1;
            b_edge_mask &= ~mask;
            t_edge_mask &= mask;
        }
    }

    uint in_word_index = bit_index % 32;
    uint b_edge_count = bitCount(b_edge_mask);
    uint t_edge_count = bitCount(t_edge_mask);

    if (gl_LocalInvocationIndex == 0) {
        result_mesh.face_count = 0;
        result_mesh.meshlet_start = 0;
    }

    barrier();

    uint face_offset = atomicAdd(result_mesh.face_count, b_edge_count + t_edge_count);

    uint shift = 0;
    [[loop]] for (uint i = 0; i < b_edge_count; ++i) {
        uint lsb = findLSB(b_edge_mask >> shift);
        shift += lsb + 1;
        uint in_strip_index = shift - 1;
        face_ids[face_offset + i] = pack(VoxelBrickFace(unswizzle_from_strip_coord(uvec3(xi, yi, in_strip_index), fi), fi * 2)).data;
    }

    shift = 0;
    [[loop]] for (uint i = 0; i < t_edge_count; ++i) {
        uint lsb = findLSB(t_edge_mask >> shift);
        shift += lsb + 1;
        uint in_strip_index = shift - 1;
        face_ids[face_offset + i + b_edge_count] = pack(VoxelBrickFace(unswizzle_from_strip_coord(uvec3(xi, yi, in_strip_index), fi), fi * 2 + 1)).data;
    }

    barrier();

    uint meshlet_n = (result_mesh.face_count + (MAX_FACES_PER_MESHLET - 1)) / MAX_FACES_PER_MESHLET;

    if (gl_LocalInvocationIndex == 0) {
        // compute size
        float size_x = brick_extent_pixels(push.uses.gpu_input, push.uses.chunks, brick_instance).x / VOXEL_BRICK_SIZE;
        bool compute_rasterize = size_x < 8;
        result_mesh.meshlet_start = allocate_meshlets(push.uses.meshlet_allocator, meshlet_n, compute_rasterize);

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
