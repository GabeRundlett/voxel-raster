#include "voxel_rasterizer.inl"
#include "allocators.glsl"
#include "culling.glsl"

DAXA_DECL_PUSH_CONSTANT(AllocateBrickInstancesPush, push)

bool is_brick_instance_already_in_drawlist(BrickInstance brick_instance) {
    // return false;
    VoxelChunk voxel_chunk = deref(advance(push.uses.chunks, brick_instance.chunk_index));
    uint flags = deref(advance(voxel_chunk.flags, brick_instance.brick_index));
    return flags != 0;
}

bool is_brick_instance_visible(BrickInstance brick_instance) {
    VoxelChunk voxel_chunk = deref(advance(push.uses.chunks, brick_instance.chunk_index));
    ivec4 pos_scl = deref(advance(voxel_chunk.pos_scl, brick_instance.brick_index));

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

    vec3 ndc_min;
    vec3 ndc_max;

    mat4 clip_to_world = deref(push.uses.gpu_input).cam.view_to_world * deref(push.uses.gpu_input).cam.sample_to_view;

    vec3 frustum_l_nrm;
    vec3 frustum_r_nrm;
    vec3 frustum_t_nrm;
    vec3 frustum_b_nrm;
    vec3 frustum_l_origin;
    vec3 frustum_r_origin;
    vec3 frustum_t_origin;
    vec3 frustum_b_origin;

    {
        vec4 fp0_h = clip_to_world * vec4(-1, -1, 1, 1);
        vec4 fp1_h = clip_to_world * vec4(-1, -1, 0.001, 1);
        vec4 fp2_h = clip_to_world * vec4(-1, +1, 0.001, 1);
        vec3 fp0 = fp0_h.xyz / fp0_h.w;
        vec3 fp1 = fp1_h.xyz / fp1_h.w;
        vec3 fp2 = fp2_h.xyz / fp2_h.w;
        frustum_l_origin = fp0;
        frustum_l_nrm = cross(fp1 - fp0, fp2 - fp0);
    }

    {
        vec4 fp0_h = clip_to_world * vec4(+1, -1, 1, 1);
        vec4 fp1_h = clip_to_world * vec4(+1, +1, 0.001, 1);
        vec4 fp2_h = clip_to_world * vec4(+1, -1, 0.001, 1);
        vec3 fp0 = fp0_h.xyz / fp0_h.w;
        vec3 fp1 = fp1_h.xyz / fp1_h.w;
        vec3 fp2 = fp2_h.xyz / fp2_h.w;
        frustum_r_origin = fp0;
        frustum_r_nrm = cross(fp1 - fp0, fp2 - fp0);
    }

    {
        vec4 fp0_h = clip_to_world * vec4(-1, -1, 1, 1);
        vec4 fp1_h = clip_to_world * vec4(+1, -1, 0.001, 1);
        vec4 fp2_h = clip_to_world * vec4(-1, -1, 0.001, 1);
        vec3 fp0 = fp0_h.xyz / fp0_h.w;
        vec3 fp1 = fp1_h.xyz / fp1_h.w;
        vec3 fp2 = fp2_h.xyz / fp2_h.w;
        frustum_t_origin = fp0;
        frustum_t_nrm = cross(fp1 - fp0, fp2 - fp0);
    }

    {
        vec4 fp0_h = clip_to_world * vec4(-1, +1, 1, 1);
        vec4 fp1_h = clip_to_world * vec4(-1, +1, 0.001, 1);
        vec4 fp2_h = clip_to_world * vec4(+1, +1, 0.001, 1);
        vec3 fp0 = fp0_h.xyz / fp0_h.w;
        vec3 fp1 = fp1_h.xyz / fp1_h.w;
        vec3 fp2 = fp2_h.xyz / fp2_h.w;
        frustum_b_origin = fp0;
        frustum_b_nrm = cross(fp1 - fp0, fp2 - fp0);
    }

    bool frustum_l_outside = true;
    bool frustum_r_outside = true;
    bool frustum_t_outside = true;
    bool frustum_b_outside = true;

    [[unroll]] for (uint vert_i = 0; vert_i < 8; ++vert_i) {
        vec4 vs_h = deref(push.uses.gpu_input).cam.world_to_view * vec4(vertices[vert_i], 1);
        vec4 cs_h = deref(push.uses.gpu_input).cam.view_to_sample * vs_h;
        vec3 p = cs_h.xyz / cs_h.w;
        if (vert_i == 0) {
            ndc_min = p;
            ndc_max = p;
        } else {
            ndc_min = min(ndc_min, p);
            ndc_max = max(ndc_max, p);
        }

        frustum_l_outside = frustum_l_outside && (dot(vertices[vert_i] - frustum_l_origin, frustum_l_nrm) > 0);
        frustum_r_outside = frustum_r_outside && (dot(vertices[vert_i] - frustum_r_origin, frustum_r_nrm) > 0);
        frustum_t_outside = frustum_t_outside && (dot(vertices[vert_i] - frustum_t_origin, frustum_t_nrm) > 0);
        frustum_b_outside = frustum_b_outside && (dot(vertices[vert_i] - frustum_b_origin, frustum_b_nrm) > 0);
    }

    bool between_raster_grid_lines = is_between_raster_grid_lines(ndc_min.xy, ndc_max.xy, vec2(deref(push.uses.gpu_input).render_size));
    const bool depth_unoccluded = !is_ndc_aabb_hiz_depth_occluded(ndc_min, ndc_max, deref(push.uses.gpu_input).render_size, deref(push.uses.gpu_input).next_lower_po2_render_size, push.uses.hiz);

    bool inside_frustum = !(frustum_l_outside || frustum_r_outside || frustum_t_outside || frustum_b_outside);

    return inside_frustum && depth_unoccluded && !between_raster_grid_lines;
}

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
void main() {
    uint chunk_index = gl_WorkGroupID.x;
    uint brick_n = deref(advance(push.uses.chunks, chunk_index)).brick_n;

    for (uint brick_index = gl_LocalInvocationIndex; brick_index < brick_n; brick_index += 128) {
        BrickInstance brick_instance;
        brick_instance.chunk_index = chunk_index;
        brick_instance.brick_index = brick_index;
        if (is_brick_instance_visible(brick_instance) && !is_brick_instance_already_in_drawlist(brick_instance)) {
            // Add to draw list
            uint brick_instance_index = allocate_brick_instances(push.uses.brick_instance_allocator, 1);
            deref(advance(push.uses.brick_instance_allocator, brick_instance_index)) = brick_instance;
            // Also mark as drawn
            VoxelChunk voxel_chunk = deref(advance(push.uses.chunks, brick_instance.chunk_index));
            deref(advance(voxel_chunk.flags, brick_instance.brick_index)) = 1;
        }
    }
}
