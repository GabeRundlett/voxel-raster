#include "defs.inl"

typedef float<2> vec2;
typedef float<3> vec3;
typedef int<3> ivec3;
typedef uint<3> uvec3;
typedef uint<2> uvec2;
#define RandomCtx const uint8 *uniform

struct MinMax {
    float min;
    float max;
};

struct DensityNrm {
    float val;
    vec3 nrm;
};

#define ARITHMETIC_OPERATOR_MINMAX(name, type, elem_type, op) \
    static inline type name(type a, type b) {                 \
        type result;                                          \
        result.min = a.min op b.min;                          \
        result.max = a.max op b.max;                          \
        return result;                                        \
    }                                                         \
    static inline type name(type v, elem_type s) {            \
        type result;                                          \
        result.min = v.min op s;                              \
        result.max = v.max op s;                              \
        return result;                                        \
    }                                                         \
    static inline type name(elem_type s, type v) {            \
        type result;                                          \
        result.min = s op v.min;                              \
        result.max = s op v.max;                              \
        return result;                                        \
    }

#define ARITHMETIC_OPERATOR_DENSITY_NRM(name, type, op) \
    static inline type name(type a, type b) {           \
        type result;                                    \
        result.val = a.val op b.val;                    \
        result.nrm = a.nrm op b.nrm;                    \
        return result;                                  \
    }

ARITHMETIC_OPERATOR_MINMAX(operator+, MinMax, int, +)
ARITHMETIC_OPERATOR_MINMAX(operator-, MinMax, int, -)
ARITHMETIC_OPERATOR_MINMAX(operator*, MinMax, int, *)
ARITHMETIC_OPERATOR_MINMAX(operator/, MinMax, int, /)

ARITHMETIC_OPERATOR_DENSITY_NRM(operator+, DensityNrm, +)
ARITHMETIC_OPERATOR_DENSITY_NRM(operator-, DensityNrm, -)

static inline float fast_random(RandomCtx random_ctx, ivec3 p) {
    p = ((p & (RANDOM_BUFFER_SIZE - 1)) + RANDOM_BUFFER_SIZE) & (RANDOM_BUFFER_SIZE - 1);
    float result = random_ctx[p.x + p.y * RANDOM_BUFFER_SIZE + p.z * RANDOM_BUFFER_SIZE * RANDOM_BUFFER_SIZE];
    return result / 255.0f;
    // RNGState rngstate;
    // seed_rng(&rngstate, p.x + p.y * RANDOM_BUFFER_SIZE + p.z * RANDOM_BUFFER_SIZE * RANDOM_BUFFER_SIZE);
    // float result = frandom(&rngstate);
    // return result - floor(result);
}

static inline vec3 floor(vec3 v) {
    vec3 result;
    result.x = floor(v.x);
    result.y = floor(v.y);
    result.z = floor(v.z);
    return result;
}

static inline vec2 round(vec2 v) {
    vec2 result;
    result.x = round(v.x);
    result.y = round(v.y);
    return result;
}

static inline vec3 fract(vec3 v) {
    vec3 result;
    result.x = v.x - floor(v.x);
    result.y = v.y - floor(v.y);
    result.z = v.z - floor(v.z);
    return result;
}

static inline vec3 abs(vec3 v) {
    vec3 result;
    result.x = abs(v.x);
    result.y = abs(v.y);
    result.z = abs(v.z);
    return result;
}

static inline float length(vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
static inline vec3 normalize(vec3 v) {
    return v / length(v);
}
static inline float dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float msign(float v) {
    return (v >= 0.0f) ? 1.0f : -1.0f;
}

vec2 map_octahedral(vec3 nor) {
    const float fac = 1.0f / (abs(nor.x) + abs(nor.y) + abs(nor.z));
    nor.x *= fac;
    nor.y *= fac;
    if (nor.z < 0.0f) {
        const vec2 temp = {nor.x, nor.y};
        nor.x = (1.0f - abs(temp.y)) * msign(temp.x);
        nor.y = (1.0f - abs(temp.x)) * msign(temp.y);
    }
    vec2 result = {nor.x, nor.y};
    return result;
}
vec3 unmap_octahedral(vec2 v) {
    vec3 nor = {v.x, v.y, 1.0f - abs(v.x) - abs(v.y)}; // Rune Stubbe's version,
    float t = max(-nor.z, 0.0f);                       // much faster than original
    nor.x += (nor.x > 0.0f) ? -t : t;                  // implementation of this
    nor.y += (nor.y > 0.0f) ? -t : t;                  // technique
    return normalize(nor);
}

float SNORM_SCALE(uint N) {
    float result = 1 << (N - 1u);
    return result - 0.5f;
}
float UNORM_SCALE(uint N) {
    float result = 1 << N;
    return result - 1.0f;
}
uint PACK_UNORM(float x, uint N) {
    return round(x * UNORM_SCALE(N));
}
float UNPACK_UNORM(uint x, uint N) {
    float result = (x) & ((1u << (N)) - 1u);
    return result / UNORM_SCALE(N);
}

#define PACK_SNORM_X2(v, N)                               \
    uvec2 d = round(SNORM_SCALE(N) + v * SNORM_SCALE(N)); \
    return d.x | (d.y << N)
#define UNPACK_SNORM_X2(d, N)            \
    uvec2 tmp0 = {(d), (d) >> N};        \
    vec2 tmp1 = tmp0 & ((1u << N) - 1u); \
    return tmp1 / SNORM_SCALE(N) - 1.0f

// #define UNORM_SCALE(N) ((1 << (N)) - 1.0f)
// #define PACK_UNORM(x, N) (round((x) * UNORM_SCALE((N))))
// #define UNPACK_UNORM(x, N) (((x) & ((1u << (N)) - 1u)) / UNORM_SCALE((N)))

uint pack_snorm_2x08(vec2 v) { PACK_SNORM_X2(v, 8); }
vec2 unpack_snorm_2x08(uint d) { UNPACK_SNORM_X2(d, 8); }

uint pack_octahedral_16(vec3 nor) { return pack_snorm_2x08(map_octahedral(nor)); }
vec3 unpack_octahedral_16(uint data) { return unmap_octahedral(unpack_snorm_2x08(data)); }

uint pack_rgb565(vec3 col) { return (PACK_UNORM(col.r, 5) << 0) | (PACK_UNORM(col.g, 6) << 5) | (PACK_UNORM(col.b, 5) << 11); }
vec3 unpack_rgb565(uint data) {
    vec3 result = {UNPACK_UNORM(data >> 0, 5), UNPACK_UNORM(data >> 5, 6), UNPACK_UNORM(data >> 11, 5)};
    return result;
}

struct Voxel {
    vec3 col;
    vec3 nrm;
};
struct PackedVoxel {
    uint data;
};
PackedVoxel pack_voxel(Voxel v) {
    PackedVoxel result;
    result.data = pack_rgb565(v.col) | (pack_octahedral_16(v.nrm) << 16);
    return result;
}
Voxel unpack_voxel(PackedVoxel v) {
    Voxel result;
    result.col = unpack_rgb565(v.data >> 0);
    result.nrm = unpack_octahedral_16(v.data >> 16);
    return result;
}


static inline DensityNrm noise(RandomCtx random_ctx, vec3 x, float scale, float amp) {
    x = x * scale;

    ivec3 p = floor(x);
    vec3 w = fract(x);
    vec3 u = w * w * (3.0f - 2.0f * w);
    vec3 du = 6.0f * w * (1.0f - w);

    const ivec3 offset_a = {0, 0, 0};
    const ivec3 offset_b = {1, 0, 0};
    const ivec3 offset_c = {0, 1, 0};
    const ivec3 offset_d = {1, 1, 0};
    const ivec3 offset_e = {0, 0, 1};
    const ivec3 offset_f = {1, 0, 1};
    const ivec3 offset_g = {0, 1, 1};
    const ivec3 offset_h = {1, 1, 1};

    float a = fast_random(random_ctx, p + offset_a);
    float b = fast_random(random_ctx, p + offset_b);
    float c = fast_random(random_ctx, p + offset_c);
    float d = fast_random(random_ctx, p + offset_d);
    float e = fast_random(random_ctx, p + offset_e);
    float f = fast_random(random_ctx, p + offset_f);
    float g = fast_random(random_ctx, p + offset_g);
    float h = fast_random(random_ctx, p + offset_h);

    float k0 = a;
    float k1 = b - a;
    float k2 = c - a;
    float k3 = e - a;
    float k4 = a - b - c + d;
    float k5 = a - c - e + g;
    float k6 = a - b - e + f;
    float k7 = -a + b + c - d + e - f - g + h;

    vec3 d0 = {k1, k2, k3};

    vec3 d1 = {u.y, u.z, u.x};
    vec3 d2 = {k4, k5, k6};

    vec3 d3 = {u.z, u.x, u.y};
    vec3 d4 = {k6, k4, k5};

    vec3 d5 = {u.y, u.z, u.x};
    vec3 d6 = {u.z, u.x, u.y};

    float result_val = k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z;
    vec3 result_nrm = du * (d0 + d1 * d2 + d3 * d4 + k7 * d5 * d6);
    DensityNrm result;
    result.val = (result_val * 2.0f - 1.0f) * amp;
    result.nrm = result_nrm * amp * scale * 2.0f;
    return result;
}

static inline MinMax minmax_noise_in_region(RandomCtx random_ctx, vec3 region_center, vec3 region_size, float scale, float amp) {
    // Use the lipschitz constant to compute min/max est. For the cubic interpolation
    // function of this noise function, said constant is 2.0 * 1.5 * scale * amp.
    //  - 2.0 * amp comes from the re-scaling of the range between -amp and amp (line 112-113)
    //  - scale comes from the re-scaling of the domain on the same line (p * scale)
    //  - 1.5 comes from the maximum abs value of the first derivative of the noise
    //    interpolation function (between 0 and 1): d/dx of 3x^2-2x^3 = 6x-6x^2.
    float lipschitz = 2.0f * 1.5f * scale * amp;
    DensityNrm dn = noise(random_ctx, region_center, scale, amp);
    float noise_val = dn.val;
    float max_dist = length(region_size);
    MinMax result;
    result.min = max(noise_val - max_dist * lipschitz, -amp);
    result.max = min(noise_val + max_dist * lipschitz, amp);
    return result;
}

static inline DensityNrm gradient_z(vec3 p, float slope, float offset) {
    DensityNrm result;
    result.val = (p.z - offset) * slope;
    vec3 nrm = {0, 0, slope};
    result.nrm = nrm;
    return result;
}
static inline MinMax minmax_gradient_z(vec3 p0, vec3 p1, float slope, float offset) {
    MinMax temp;
    temp.min = (p0.z - offset) * slope;
    temp.max = (p1.z - offset) * slope;
    MinMax result;
    result.min = min(temp.min, temp.max);
    result.max = max(temp.min, temp.max);
    return result;
}

const uniform float NOISE_PERSISTENCE = 0.20f;
const uniform float NOISE_LACUNARITY = 4.0f;
const uniform float NOISE_SCALE = 0.05f;
const uniform float NOISE_AMPLITUDE = 20.0f;
const uniform uint NOISE_OCTAVES = 5;

static inline float voxel_value(RandomCtx random_ctx, vec3 pos) {
    DensityNrm result;
    // pos = fract(pos / 4.0f) * 4.0f - 2.0f;
    // result += length(pos) - 1.9f;
    result = gradient_z(pos, -1, 24.0f);
    {
        float noise_persistence = NOISE_PERSISTENCE;
        float noise_lacunarity = NOISE_LACUNARITY;
        float noise_scale = NOISE_SCALE;
        float noise_amplitude = NOISE_AMPLITUDE;
        for (uint i = 0; i < NOISE_OCTAVES; ++i) {
            // pos = m * pos;
            result = result + noise(random_ctx, pos, noise_scale, noise_amplitude);
            noise_scale *= noise_lacunarity;
            noise_amplitude *= noise_persistence;
        }
    }
    return result.val;
}
static inline DensityNrm voxel_nrm(RandomCtx random_ctx, vec3 pos) {
    DensityNrm result;
    // pos = fract(pos / 4.0f) * 4.0f - 2.0f;
    // result.val += length(pos) - 2.0f;
    // result.nrm += normalize(pos);

    result = gradient_z(pos, -1, 24.0f);
    {
        float noise_persistence = NOISE_PERSISTENCE;
        float noise_lacunarity = NOISE_LACUNARITY;
        float noise_scale = NOISE_SCALE;
        float noise_amplitude = NOISE_AMPLITUDE;
        // auto inv = mat3(1, 0, 0, 0, 1, 0, 0, 0, 1);
        for (uint i = 0; i < NOISE_OCTAVES; ++i) {
            // pos = m * pos;
            // inv = mi * inv;
            DensityNrm dn = noise(random_ctx, pos, noise_scale, noise_amplitude);
            result.val += dn.val;
            result.nrm += /* inv * */ dn.nrm;
            noise_scale *= noise_lacunarity;
            noise_amplitude *= noise_persistence;
        }
    }
    result.nrm = normalize(result.nrm);
    return result;
}
static inline MinMax voxel_minmax_value(RandomCtx random_ctx, vec3 p0, vec3 p1) {
    MinMax result = {0, 0};
    // result += glm::vec2(-1, 1);
    result = result + minmax_gradient_z(p0, p1, -1, 24.0f);
    {
        float noise_persistence = NOISE_PERSISTENCE;
        float noise_lacunarity = NOISE_LACUNARITY;
        float noise_scale = NOISE_SCALE;
        float noise_amplitude = NOISE_AMPLITUDE;
        for (uint i = 0; i < NOISE_OCTAVES; ++i) {
            // p0 = m * p0;
            // p1 = m * p1;
            result = result + minmax_noise_in_region(random_ctx, (p0 + p1) * 0.5f, abs(p1 - p0), noise_scale, noise_amplitude);
            noise_scale *= noise_lacunarity;
            noise_amplitude *= noise_persistence;
        }
    }
    return result;
}

export void generate_bitmask(
    uniform int brick_xi, uniform int brick_yi, uniform int brick_zi,
    uniform int chunk_xi, uniform int chunk_yi, uniform int chunk_zi,
    uniform int level_i, uniform uint bits[], uint *uniform metadata,
    RandomCtx random_ctx) {

    bool has_voxel = false;
    bool has_air_nx = false;
    bool has_air_px = false;
    bool has_air_ny = false;
    bool has_air_py = false;
    bool has_air_nz = false;
    bool has_air_pz = false;

    foreach (word_i = 0 ... (VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE / 32)) {
        uint word_result = 0;
        for (uniform uint in_word_i = 0; in_word_i < 32; ++in_word_i) {
            uint i = word_i * 32 + in_word_i;
            uint xi = i & ((1 << VOXEL_BRICK_SIZE_LOG2) - 1);
            uint yi = (i >> VOXEL_BRICK_SIZE_LOG2) & ((1 << VOXEL_BRICK_SIZE_LOG2) - 1);
            uint zi = (i >> (VOXEL_BRICK_SIZE_LOG2 * 2)) & ((1 << VOXEL_BRICK_SIZE_LOG2) - 1);

            vec3 pos;
            pos.x = (xi + brick_xi * VOXEL_BRICK_SIZE + chunk_xi * VOXEL_CHUNK_SIZE) << level_i;
            pos.y = (yi + brick_yi * VOXEL_BRICK_SIZE + chunk_yi * VOXEL_CHUNK_SIZE) << level_i;
            pos.z = (zi + brick_zi * VOXEL_BRICK_SIZE + chunk_zi * VOXEL_CHUNK_SIZE) << level_i;

            pos.x = (pos.x + 0.5f) / 16.0f;
            pos.y = (pos.y + 0.5f) / 16.0f;
            pos.z = (pos.z + 0.5f) / 16.0f;

            uint value = voxel_value(random_ctx, pos) < 0.0f;
            word_result |= value << in_word_i;

            has_voxel = has_voxel || (value != 0);
            has_air_nx = has_air_nx || (xi == 0 && value == 0);
            has_air_px = has_air_px || (xi == (VOXEL_BRICK_SIZE - 1) && value == 0);
            has_air_ny = has_air_ny || (yi == 0 && value == 0);
            has_air_py = has_air_py || (yi == (VOXEL_BRICK_SIZE - 1) && value == 0);
            has_air_nz = has_air_nz || (zi == 0 && value == 0);
            has_air_pz = has_air_pz || (zi == (VOXEL_BRICK_SIZE - 1) && value == 0);
        }
        bits[word_i] = word_result;
    }

    if (packmask(has_voxel) != 0) {
        *metadata |= (1 << 12);
    }
    if (packmask(has_air_nx) != 0) {
        *metadata |= (1 << 6);
    }
    if (packmask(has_air_px) != 0) {
        *metadata |= (1 << 9);
    }
    if (packmask(has_air_ny) != 0) {
        *metadata |= (1 << 7);
    }
    if (packmask(has_air_py) != 0) {
        *metadata |= (1 << 10);
    }
    if (packmask(has_air_nz) != 0) {
        *metadata |= (1 << 8);
    }
    if (packmask(has_air_pz) != 0) {
        *metadata |= (1 << 11);
    }

    // *metadata = ((has_voxel_bits != 0) << 12) |
    //     ((has_air_nx_bits != 0) << 6) |
    //     ((has_air_px_bits != 0) << 9) |
    //     ((has_air_ny_bits != 0) << 7) |
    //     ((has_air_py_bits != 0) << 10) |
    //     ((has_air_nz_bits != 0) << 8) |
    //     ((has_air_pz_bits != 0) << 11);
}

export void generate_attributes(
    uniform int brick_xi, uniform int brick_yi, uniform int brick_zi,
    uniform int chunk_xi, uniform int chunk_yi, uniform int chunk_zi,
    uniform int level_i, uniform uint packed_voxels[],
    RandomCtx random_ctx) {
    const uniform vec3 UP = {0, 0, -1};
    const uniform vec3 GRASS_COL = {12, 163, 7};
    const uniform vec3 DIRT_COL = {112, 62, 30};

    foreach (word_i = 0 ... (VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE / 32)) {
        for (uniform uint in_word_i = 0; in_word_i < 32; ++in_word_i) {
            uint voxel_index = word_i * 32 + in_word_i;
            uint xi = voxel_index & ((1 << VOXEL_BRICK_SIZE_LOG2) - 1);
            uint yi = (voxel_index >> VOXEL_BRICK_SIZE_LOG2) & ((1 << VOXEL_BRICK_SIZE_LOG2) - 1);
            uint zi = (voxel_index >> (VOXEL_BRICK_SIZE_LOG2 * 2)) & ((1 << VOXEL_BRICK_SIZE_LOG2) - 1);

            vec3 pos;
            pos.x = (xi + brick_xi * VOXEL_BRICK_SIZE + chunk_xi * VOXEL_CHUNK_SIZE) << level_i;
            pos.y = (yi + brick_yi * VOXEL_BRICK_SIZE + chunk_yi * VOXEL_CHUNK_SIZE) << level_i;
            pos.z = (zi + brick_zi * VOXEL_BRICK_SIZE + chunk_zi * VOXEL_CHUNK_SIZE) << level_i;

            pos.x = (pos.x + 0.5f) / 16.0f;
            pos.y = (pos.y + 0.5f) / 16.0f;
            pos.z = (pos.z + 0.5f) / 16.0f;

            DensityNrm dn = voxel_nrm(random_ctx, pos);
            Voxel voxel;
            if (dot(dn.nrm, UP) > 0.5f && dn.val > -2.5f) {
                voxel.col = GRASS_COL / 255.0f;
            } else {
                voxel.col = DIRT_COL / 255.0f;
            }
            voxel.nrm = dn.nrm;
            PackedVoxel packed_voxel = pack_voxel(voxel);
            packed_voxels[voxel_index] = packed_voxel.data;
        }
    }
}
