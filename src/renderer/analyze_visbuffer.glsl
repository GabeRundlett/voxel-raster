#include <shared.inl>
#include <renderer/visbuffer.glsl>
#include <voxels/voxel_mesh.glsl>
#include <renderer/meshlet_allocator.glsl>

DAXA_DECL_PUSH_CONSTANT(AnalyzeVisbufferPush, push)

void write_visible_brick(uint brick_instance_index) {
    // uint word_index = brick_instance_index / 32;
    // uint in_word_index = brick_instance_index % 32;
    uint visibility_mask = 1;
    uint prev_bits = atomicOr(deref(push.uses.brick_visibility_bits[brick_instance_index]), visibility_mask);
    if ((prev_bits & visibility_mask) == 0) {
        // We're the first thread to set the flag, so we can write ourselves out
        uint visible_brick_instance_index = allocate_brick_instances(push.uses.visible_brick_instance_allocator, 1);
        // it should be impossible for this to ever be bad, but we'll check anyway.
        if (visible_brick_instance_index != 0) {
            BrickInstance brick_instance = deref(push.uses.brick_instance_allocator[brick_instance_index]);
            deref(push.uses.visible_brick_instance_allocator[visible_brick_instance_index]) = brick_instance;
            // Also mark as submitted for drawing
            VoxelChunk voxel_chunk = deref(push.uses.chunks[brick_instance.chunk_index]);
            deref(voxel_chunk.flags[brick_instance.brick_index]) = 1;
        }
    }
}

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main() {
    // Reswizzle threads into 8x8 blocks:
    uvec2 thread_index = gl_WorkGroupID.xy * 8 + uvec2(gl_LocalInvocationIndex % 8, gl_LocalInvocationIndex / 8);
    // Each thread handles 2x2 pixels:
    ivec2 sample_index = ivec2(thread_index * 2);

    uvec4 visbuffer_ids;
    visbuffer_ids[0] = texelFetch(daxa_utexture2D(push.uses.visbuffer), sample_index + ivec2(0, 0), 0).r;
    visbuffer_ids[1] = texelFetch(daxa_utexture2D(push.uses.visbuffer), sample_index + ivec2(0, 1), 0).r;
    visbuffer_ids[2] = texelFetch(daxa_utexture2D(push.uses.visbuffer), sample_index + ivec2(1, 0), 0).r;
    visbuffer_ids[3] = texelFetch(daxa_utexture2D(push.uses.visbuffer), sample_index + ivec2(1, 1), 0).r;

    uvec4 brick_instance_indices = uvec4(0);
    uint list_mask = 0;

    [[unroll]] for (uint i = 0; i < 4; ++i) {
        uint visbuffer_id = visbuffer_ids[i];
        VisbufferPayload payload = unpack(PackedVisbufferPayload(visbuffer_id));
        VoxelMeshlet meshlet = deref(push.uses.meshlet_allocator[payload.meshlet_id]);
        PackedVoxelBrickFace packed_face = meshlet.faces[payload.face_id];
        VoxelBrickFace face = unpack(packed_face);
        VoxelMeshletMetadata metadata = deref(push.uses.meshlet_metadata[payload.meshlet_id]);
        brick_instance_indices[i] = metadata.brick_instance_index;

        // bool list_entry_valid = (visbuffer_id != INVALID_MESHLET_INDEX);
        // list_mask |= (uint(list_entry_valid) << i);

        bool list_entry_valid = true;
        list_entry_valid = list_entry_valid && (visbuffer_id != INVALID_MESHLET_INDEX);
        // list_entry_valid = list_entry_valid && (meshlet_instance_indices[i] < MAX_MESHLET_INSTANCES);
        list_mask = list_mask | (list_entry_valid ? (1u << i) : 0u);
    }

    uint assigned_meshlet_index = ~0;
    while (subgroupAny(assigned_meshlet_index == ~0) && subgroupAny(list_mask != 0)) {
        // Each iteration, all the threads pick a candidate they want to vote for,
        // A single meshlet is chosen every iteration.
        bool thread_active = list_mask != 0;
        const uint voting_candidate = thread_active ? brick_instance_indices[findLSB(list_mask)] : 0;
        uint elected_meshlet_instance_index = subgroupShuffle(voting_candidate, subgroupBallotFindLSB(subgroupBallot(thread_active)));

        // Now that a meshlet is voted for, we remove meshlets from each threads lists here.
        [[unroll]] for (uint i = 0; i < 4; ++i) {
            if (brick_instance_indices[i] == elected_meshlet_instance_index) {
                list_mask &= ~(1u << i);
            }
        }

        // Now a single thread is choosen to pick this voted meshlet and its triangle mask to write out later.
        if (assigned_meshlet_index == ~0 && subgroupElect()) {
            assigned_meshlet_index = elected_meshlet_instance_index;
        }
    }

    // Mark and write assigned meshlet.
    if (assigned_meshlet_index != ~0) {
        write_visible_brick(assigned_meshlet_index);
    }

    // When there are more unique meshlets per warp then warpsize,
    // the thread needs to write out its remaining meshlet instance indices.
    // This is done more efficiently with a non scalarized loop.
    [[loop]] while (list_mask != 0) {
        uint lsb = findLSB(list_mask);
        uint brick_instance_index = brick_instance_indices[lsb];
        list_mask &= ~(1 << lsb);
        write_visible_brick(brick_instance_index);
    }
}
