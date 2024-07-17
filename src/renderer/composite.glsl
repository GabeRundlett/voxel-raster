#include <shared.inl>
#include <voxels/pack_unpack.inl>

DAXA_DECL_PUSH_CONSTANT(CompositePush, push)

layout(location = 0) out vec4 f_out;

const vec3 SKY_COL = vec3(20, 20, 255) / 255;
const vec3 SUN_COL = vec3(0.9, 0.7, 0.5) * 2;
const vec3 SUN_DIR = normalize(vec3(-1.7, 2.4, 3.1));

void main() {
    vec3 albedo = texelFetch(daxa_texture2D(push.uses.color), ivec2(gl_FragCoord.xy), 0).rgb;
    float depth = texelFetch(daxa_texture2D(push.uses.depth), ivec2(gl_FragCoord.xy), 0).r;
    float shadow_mask = texelFetch(daxa_texture2D(push.uses.shadow_mask), ivec2(gl_FragCoord.xy), 0).r;
    vec3 nrm = unmap_octahedral(texelFetch(daxa_texture2D(push.uses.normal), ivec2(gl_FragCoord.xy), 0).xy);

    if (depth == 0) {
        f_out = vec4(SKY_COL, 1);
    } else {
        vec3 diffuse = vec3(0);
        // diffuse += vec3(1);
        diffuse += max(0.0, dot(nrm, SUN_DIR)) * SUN_COL * shadow_mask;
        diffuse += max(0.0, dot(nrm, normalize(vec3(0, 0, 1))) * 0.4 + 0.6) * SKY_COL;
        f_out = vec4(albedo * diffuse, 1);
    }

}
