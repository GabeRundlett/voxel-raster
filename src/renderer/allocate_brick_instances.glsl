#include <shared.inl>
#include <renderer/meshlet_allocator.glsl>
#include <renderer/hiz.glsl>

DAXA_DECL_PUSH_CONSTANT(AllocateBrickInstancesPush, push)

bool is_brick_instance_visible(BrickInstance brick_instance) {
    VoxelChunk voxel_chunk = deref(push.uses.chunks[brick_instance.chunk_index]);
    ivec4 pos_scl = deref(voxel_chunk.pos_scl[brick_instance.brick_index]);

    vec3 p0 = ivec3(voxel_chunk.pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(0);
    vec3 p1 = ivec3(voxel_chunk.pos) * int(VOXEL_CHUNK_SIZE) + pos_scl.xyz * int(VOXEL_BRICK_SIZE) + ivec3(VOXEL_BRICK_SIZE);
    int scl = pos_scl.w + 8;
#define SCL (float(1 << scl) / float(1 << 8))
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

    mat4 world_to_clip = deref(push.uses.gpu_input).cam.view_to_clip * deref(push.uses.gpu_input).cam.world_to_view;
    vec3 ndc_min = vec3(+2);
    vec3 ndc_max = vec3(-2);

    [[unroll]] for (uint vert_i = 0; vert_i < 8; ++vert_i) {
        vec4 p_h = world_to_clip * vec4(vertices[vert_i], 1);
        vec3 p = p_h.xyz / p_h.w;
        ndc_min = min(ndc_min, p);
        ndc_max = max(ndc_max, p);
    }

    const bool depth_unoccluded = !is_ndc_aabb_hiz_depth_occluded(ndc_min, ndc_max, deref(push.uses.gpu_input).next_lower_po2_render_size, push.uses.hiz);

    bool inside_frustum = !(any(greaterThan(ndc_min.xy, vec2(1))) || any(lessThan(ndc_max.xy, vec2(-1))));

    return inside_frustum && depth_unoccluded;
}

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
void main() {
    uint chunk_index = gl_WorkGroupID.x;
    uint brick_n = deref(push.uses.chunks[chunk_index]).brick_n;

    for (uint brick_index = gl_LocalInvocationIndex; brick_index < brick_n; brick_index += 128) {
        BrickInstance brick_instance;
        brick_instance.chunk_index = chunk_index;
        brick_instance.brick_index = brick_index;
        if (is_brick_instance_visible(brick_instance)) {
            uint brick_instance_index = allocate_brick_instances(push.uses.brick_instance_allocator, 1);
            deref(push.uses.brick_instance_allocator[brick_instance_index]) = brick_instance;
        }
    }
}
