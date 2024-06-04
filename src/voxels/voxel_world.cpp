#include "voxel_world.hpp"
#include "voxels/pack_unpack.inl"
#include "voxels/voxel_mesh.inl"

#include <glm/common.hpp>
#include <glm/glm.hpp>

#include <gvox/gvox.h>
#include <gvox/streams/input/byte_buffer.h>
#include <gvox/containers/raw.h>

#include <renderer/renderer.hpp>

#include <fstream>
#include <array>
#include <vector>
#include <thread>

struct BrickMetadata {
    uint32_t exposed_nx : 1 {};
    uint32_t exposed_ny : 1 {};
    uint32_t exposed_nz : 1 {};
    uint32_t exposed_px : 1 {};
    uint32_t exposed_py : 1 {};
    uint32_t exposed_pz : 1 {};
    uint32_t has_air_nx : 1 {};
    uint32_t has_air_ny : 1 {};
    uint32_t has_air_nz : 1 {};
    uint32_t has_air_px : 1 {};
    uint32_t has_air_py : 1 {};
    uint32_t has_air_pz : 1 {};
    uint32_t has_voxel : 1 {};
};

struct Chunk {
    std::array<VoxelBrickBitmask, BRICKS_PER_CHUNK> voxel_brick_bitmasks{};
    std::array<std::unique_ptr<VoxelAttribBrick>, BRICKS_PER_CHUNK> voxel_brick_attribs{};
    std::vector<VoxelBrickBitmask> surface_brick_bitmasks;
    std::vector<glm::ivec4> surface_brick_positions;
    std::vector<VoxelAttribBrick> surface_attrib_bricks;
    renderer::Chunk render_chunk = nullptr;
    glm::vec3 pos;
    bool bricks_changed;

    Chunk() {
        renderer::init(render_chunk);
    }
    ~Chunk() {
        if (render_chunk != nullptr) {
            renderer::deinit(render_chunk);
        }
    }
};

struct voxel_world::State {
    std::array<std::unique_ptr<Chunk>, MAX_CHUNK_COUNT> chunks;
    float prev_time;

    std::thread test_chunk_thread;
    bool launch_update;
};

struct DensityNrm {
    float val;
    glm::vec3 nrm;
};

#include <random>

const auto RANDOM_BUFFER_SIZE = size_t{256};
const auto RANDOM_SEED = 0;
const auto RANDOM_VALUES = []() {
    auto result = std::vector<uint8_t>{};
    result.resize(RANDOM_BUFFER_SIZE * RANDOM_BUFFER_SIZE * RANDOM_BUFFER_SIZE);
    auto rng = std::mt19937_64(RANDOM_SEED);
    auto dist = std::uniform_int_distribution<std::mt19937::result_type>(0, 255);
    for (auto &val : result) {
        val = dist(rng) & 0xff;
    }
    return result;
}();

float fast_random(glm::ivec3 p) {
    p = ((p % ivec3(RANDOM_BUFFER_SIZE)) + ivec3(RANDOM_BUFFER_SIZE)) % ivec3(RANDOM_BUFFER_SIZE);
    return float(RANDOM_VALUES[p.x + p.y * RANDOM_BUFFER_SIZE + p.z * RANDOM_BUFFER_SIZE * RANDOM_BUFFER_SIZE]) / 255.0f;
}

DensityNrm noise(glm::vec3 x, float scale, float amp) {
    x = x * scale;

    glm::ivec3 p = floor(x);
    glm::vec3 w = fract(x);
    glm::vec3 u = w * w * (3.0f - 2.0f * w);
    glm::vec3 du = 6.0f * w * (1.0f - w);

    float a = fast_random(p + ivec3(0, 0, 0));
    float b = fast_random(p + ivec3(1, 0, 0));
    float c = fast_random(p + ivec3(0, 1, 0));
    float d = fast_random(p + ivec3(1, 1, 0));
    float e = fast_random(p + ivec3(0, 0, 1));
    float f = fast_random(p + ivec3(1, 0, 1));
    float g = fast_random(p + ivec3(0, 1, 1));
    float h = fast_random(p + ivec3(1, 1, 1));

    float k0 = a;
    float k1 = b - a;
    float k2 = c - a;
    float k3 = e - a;
    float k4 = a - b - c + d;
    float k5 = a - c - e + g;
    float k6 = a - b - e + f;
    float k7 = -a + b + c - d + e - f - g + h;

    float result = k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z;
    auto result_nrm = du * (glm::vec3(k1, k2, k3) + glm::vec3(u.y, u.z, u.x) * glm::vec3(k4, k5, k6) + glm::vec3(u.z, u.x, u.y) * glm::vec3(k6, k4, k5) + k7 * glm::vec3(u.y, u.z, u.x) * glm::vec3(u.z, u.x, u.y));
    return DensityNrm((result * 2.0f - 1.0f) * amp, result_nrm * amp * scale * 2.0f);
}

glm::vec2 minmax_noise_in_region(glm::vec3 region_center, glm::vec3 region_size, float scale, float amp) {
    // Use the lipschitz constant to compute min/max est. For the cubic interpolation
    // function of this noise function, said constant is 2.0 * 1.5 * scale * amp.
    //  - 2.0 * amp comes from the re-scaling of the range between -amp and amp (line 112-113)
    //  - scale comes from the re-scaling of the domain on the same line (p * scale)
    //  - 1.5 comes from the maximum abs value of the first derivative of the noise
    //    interpolation function (between 0 and 1): d/dx of 3x^2-2x^3 = 6x-6x^2.
    float lipschitz = 2.0f * 1.5f * scale * amp;
    float noise_val = noise(region_center, scale, amp).val;
    float max_dist = length(region_size);
    return glm::vec2(std::max(noise_val - max_dist * lipschitz, -amp), std::min(noise_val + max_dist * lipschitz, amp));
}

DensityNrm gradient_z(glm::vec3 p, float slope, float offset) {
    return DensityNrm((p.z - offset) * slope, glm::vec3(0, 0, slope));
}
glm::vec2 minmax_gradient_z(glm::vec3 p0, glm::vec3 p1, float slope, float offset) {
    auto result = glm::vec2((p0.z - offset) * slope, (p1.z - offset) * slope);
    return glm::vec2(std::min(result[0], result[1]), std::max(result[0], result[1]));
}

constexpr int32_t CHUNK_NX = 8;
constexpr int32_t CHUNK_NY = 8;
constexpr int32_t CHUNK_NZ = 4;
constexpr int32_t CHUNK_LEVELS = 1;
constexpr mat3 m = mat3(0.00, 0.80, 0.60,
                        -0.80, 0.36, -0.48,
                        -0.60, -0.48, 0.64);
const mat3 mi = inverse(m);

const float NOISE_PERSISTENCE = 0.20f;
const float NOISE_LACUNARITY = 4.0f;
const float NOISE_SCALE = 0.05f;
const float NOISE_AMPLITUDE = 20.0f;

auto get_brick_metadata(std::unique_ptr<Chunk> &chunk, auto brick_index) -> BrickMetadata & {
    return *reinterpret_cast<BrickMetadata *>(&chunk->voxel_brick_bitmasks[brick_index].metadata);
}

auto voxel_value(glm::vec3 pos) {
    auto result = 0.0f;
    // pos = fract(pos / 4.0f) * 4.0f - 2.0f;
    // result += length(pos) - 1.9f;
    result += gradient_z(pos, -1, 24.0f).val;
    {
        float noise_persistence = NOISE_PERSISTENCE;
        float noise_lacunarity = NOISE_LACUNARITY;
        float noise_scale = NOISE_SCALE;
        float noise_amplitude = NOISE_AMPLITUDE;
        for (uint32_t i = 0; i < 5; ++i) {
            pos = m * pos;
            result += noise(pos, noise_scale, noise_amplitude).val;
            noise_scale *= noise_lacunarity;
            noise_amplitude *= noise_persistence;
        }
    }
    return result;
}
auto voxel_nrm(glm::vec3 pos) {
    auto result = DensityNrm(0, glm::vec3(0));
    // pos = fract(pos / 4.0f) * 4.0f - 2.0f;
    // result.val += length(pos) - 2.0f;
    // result.nrm += normalize(pos);

    {
        auto dn = gradient_z(pos, -1, 24.0f);
        result.val += dn.val;
        result.nrm += dn.nrm;
    }
    {
        float noise_persistence = NOISE_PERSISTENCE;
        float noise_lacunarity = NOISE_LACUNARITY;
        float noise_scale = NOISE_SCALE;
        float noise_amplitude = NOISE_AMPLITUDE;
        auto inv = mat3(1, 0, 0, 0, 1, 0, 0, 0, 1);
        for (uint32_t i = 0; i < 5; ++i) {
            pos = m * pos;
            inv = mi * inv;
            auto dn = noise(pos, noise_scale, noise_amplitude);
            result.val += dn.val;
            result.nrm += inv * dn.nrm;
            noise_scale *= noise_lacunarity;
            noise_amplitude *= noise_persistence;
        }
    }
    result.nrm = normalize(result.nrm);
    return result;
}
auto voxel_minmax_value(glm::vec3 p0, glm::vec3 p1) {
    auto result = glm::vec2(0);
    // result += glm::vec2(-1, 1);
    result += minmax_gradient_z(p0, p1, -1, 24.0f);
    {
        float noise_persistence = NOISE_PERSISTENCE;
        float noise_lacunarity = NOISE_LACUNARITY;
        float noise_scale = NOISE_SCALE;
        float noise_amplitude = NOISE_AMPLITUDE;
        for (uint32_t i = 0; i < 5; ++i) {
            p0 = m * p0;
            p1 = m * p1;
            result += minmax_noise_in_region((p0 + p1) * 0.5f, abs(p1 - p0), noise_scale, noise_amplitude);
            noise_scale *= noise_lacunarity;
            noise_amplitude *= noise_persistence;
        }
    }
    return result;
}

auto generate_chunk(voxel_world::VoxelWorld self, int32_t chunk_xi, int32_t chunk_yi, int32_t chunk_zi, int32_t level) {
    if (level > 0 && chunk_xi < CHUNK_NX / 2 && chunk_yi < CHUNK_NY / 2 && chunk_zi < CHUNK_NZ / 2) {
        return;
    }

    {
        auto p0 = glm::vec3{
            (float((chunk_xi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f,
            (float((chunk_yi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f,
            (float((chunk_zi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f,
        };
        auto p1 = p0 + BRICK_CHUNK_SIZE * VOXEL_BRICK_SIZE / 16.0f;
        auto minmax = voxel_minmax_value(p0, p1);
        if (minmax[0] >= 0.0f || minmax[1] < 0.0f) {
            // uniform
            if (minmax[0] < 0.0f) {
                // inside
            } else {
                // outside
                return;
            }
        }
    }

    int32_t chunk_index = chunk_xi + chunk_yi * CHUNK_NX + chunk_zi * CHUNK_NX * CHUNK_NY + level * CHUNK_NX * CHUNK_NY * CHUNK_NZ;
    auto &chunk = self->chunks[chunk_index];
    chunk = std::make_unique<Chunk>();
    chunk->pos = {chunk_xi, chunk_yi, chunk_zi};

    for (int32_t brick_zi = 0; brick_zi < BRICK_CHUNK_SIZE; ++brick_zi) {
        for (int32_t brick_yi = 0; brick_yi < BRICK_CHUNK_SIZE; ++brick_yi) {
            for (int32_t brick_xi = 0; brick_xi < BRICK_CHUNK_SIZE; ++brick_xi) {
                auto brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                auto &brick_metadata = get_brick_metadata(chunk, brick_index);
                auto &bitmask = chunk->voxel_brick_bitmasks[brick_index];

                brick_metadata = {};
                bitmask = {};

                // determine if brick is uniform

                {
                    auto p0 = glm::vec3{
                        (float((brick_xi * VOXEL_BRICK_SIZE + chunk_xi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f,
                        (float((brick_yi * VOXEL_BRICK_SIZE + chunk_yi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f,
                        (float((brick_zi * VOXEL_BRICK_SIZE + chunk_zi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f,
                    };
                    auto p1 = p0 + VOXEL_BRICK_SIZE / 16.0f;
                    auto minmax = voxel_minmax_value(p0, p1);
                    if (minmax[0] >= 0.0f || minmax[1] < 0.0f) {
                        // uniform
                        if (minmax[0] < 0.0f) {
                            // inside
                            brick_metadata.has_voxel = true;
                            for (auto &word : bitmask.bits) {
                                word = 0xffffffff;
                            }
                        } else {
                            // outside
                            brick_metadata.has_air_px = true;
                            brick_metadata.has_air_nx = true;
                            brick_metadata.has_air_py = true;
                            brick_metadata.has_air_ny = true;
                            brick_metadata.has_air_pz = true;
                            brick_metadata.has_air_nz = true;
                        }
                        continue;
                    }
                }

                for (uint32_t zi = 0; zi < VOXEL_BRICK_SIZE; ++zi) {
                    for (uint32_t yi = 0; yi < VOXEL_BRICK_SIZE; ++yi) {
                        for (uint32_t xi = 0; xi < VOXEL_BRICK_SIZE; ++xi) {
                            float x = (float((xi + brick_xi * VOXEL_BRICK_SIZE + chunk_xi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f;
                            float y = (float((yi + brick_yi * VOXEL_BRICK_SIZE + chunk_yi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f;
                            float z = (float((zi + brick_zi * VOXEL_BRICK_SIZE + chunk_zi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f;

                            uint32_t value = voxel_value(glm::vec3(x, y, z)) < 0.0f ? 1 : 0;

                            if (value != 0) {
                                brick_metadata.has_voxel = true;
                            }
                            if (xi == 0 && value == 0) {
                                brick_metadata.has_air_nx = true;
                            } else if (xi == (VOXEL_BRICK_SIZE - 1) && value == 0) {
                                brick_metadata.has_air_px = true;
                            }
                            if (yi == 0 && value == 0) {
                                brick_metadata.has_air_ny = true;
                            } else if (yi == (VOXEL_BRICK_SIZE - 1) && value == 0) {
                                brick_metadata.has_air_py = true;
                            }
                            if (zi == 0 && value == 0) {
                                brick_metadata.has_air_nz = true;
                            } else if (zi == (VOXEL_BRICK_SIZE - 1) && value == 0) {
                                brick_metadata.has_air_pz = true;
                            }

                            uint32_t voxel_index = xi + yi * VOXEL_BRICK_SIZE + zi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
                            uint32_t voxel_word_index = voxel_index / 32;
                            uint32_t voxel_in_word_index = voxel_index % 32;
                            bitmask.bits[voxel_word_index] |= uint32_t(value) << voxel_in_word_index;
                        }
                    }
                }
            }
        }
    }
}

auto generate_chunk2(voxel_world::VoxelWorld self, int32_t chunk_xi, int32_t chunk_yi, int32_t chunk_zi, int32_t level, bool update = true) {
    int32_t chunk_index = chunk_xi + chunk_yi * CHUNK_NX + chunk_zi * CHUNK_NX * CHUNK_NY + level * CHUNK_NX * CHUNK_NY * CHUNK_NZ;
    auto &chunk = self->chunks[chunk_index];
    if (!chunk) {
        return;
    }

    for (int32_t brick_zi = 0; brick_zi < BRICK_CHUNK_SIZE; ++brick_zi) {
        for (int32_t brick_yi = 0; brick_yi < BRICK_CHUNK_SIZE; ++brick_yi) {
            for (int32_t brick_xi = 0; brick_xi < BRICK_CHUNK_SIZE; ++brick_xi) {
                auto brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                auto &brick_metadata = get_brick_metadata(chunk, brick_index);
                auto &bitmask = chunk->voxel_brick_bitmasks[brick_index];

                brick_metadata.exposed_nx = false;
                brick_metadata.exposed_px = false;
                brick_metadata.exposed_ny = false;
                brick_metadata.exposed_py = false;
                brick_metadata.exposed_nz = false;
                brick_metadata.exposed_pz = false;

                auto const *neighbor_bitmask_nx = (VoxelBrickBitmask const *)nullptr;
                auto const *neighbor_bitmask_px = (VoxelBrickBitmask const *)nullptr;
                auto const *neighbor_bitmask_ny = (VoxelBrickBitmask const *)nullptr;
                auto const *neighbor_bitmask_py = (VoxelBrickBitmask const *)nullptr;
                auto const *neighbor_bitmask_nz = (VoxelBrickBitmask const *)nullptr;
                auto const *neighbor_bitmask_pz = (VoxelBrickBitmask const *)nullptr;

                if (brick_xi != 0) {
                    auto neighbor_brick_index = (brick_xi - 1) + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                    auto &neighbor_brick_metadata = get_brick_metadata(chunk, neighbor_brick_index);
                    brick_metadata.exposed_nx = neighbor_brick_metadata.has_air_px;
                    neighbor_bitmask_nx = &chunk->voxel_brick_bitmasks[neighbor_brick_index];
                } else if (chunk_xi != 0) {
                    int32_t neighbor_chunk_index = (chunk_xi - 1) + chunk_yi * CHUNK_NX + chunk_zi * CHUNK_NX * CHUNK_NY + level * CHUNK_NX * CHUNK_NY * CHUNK_NZ;
                    auto &neighbor_chunk = self->chunks[neighbor_chunk_index];
                    if (neighbor_chunk) {
                        auto neighbor_brick_index = (BRICK_CHUNK_SIZE - 1) + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_chunk, neighbor_brick_index);
                        brick_metadata.exposed_nx = neighbor_brick_metadata.has_air_px;
                        neighbor_bitmask_nx = &neighbor_chunk->voxel_brick_bitmasks[neighbor_brick_index];
                    } else {
                        brick_metadata.exposed_nx = true;
                    }
                }
                if (brick_yi != 0) {
                    auto neighbor_brick_index = brick_xi + (brick_yi - 1) * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                    auto &neighbor_brick_metadata = get_brick_metadata(chunk, neighbor_brick_index);
                    brick_metadata.exposed_ny = neighbor_brick_metadata.has_air_py;
                    neighbor_bitmask_ny = &chunk->voxel_brick_bitmasks[neighbor_brick_index];
                } else if (chunk_yi != 0) {
                    int32_t neighbor_chunk_index = chunk_xi + (chunk_yi - 1) * CHUNK_NX + chunk_zi * CHUNK_NX * CHUNK_NY + level * CHUNK_NX * CHUNK_NY * CHUNK_NZ;
                    auto &neighbor_chunk = self->chunks[neighbor_chunk_index];
                    if (neighbor_chunk) {
                        auto neighbor_brick_index = brick_xi + (BRICK_CHUNK_SIZE - 1) * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_chunk, neighbor_brick_index);
                        brick_metadata.exposed_ny = neighbor_brick_metadata.has_air_py;
                        neighbor_bitmask_ny = &neighbor_chunk->voxel_brick_bitmasks[neighbor_brick_index];
                    } else {
                        brick_metadata.exposed_ny = true;
                    }
                }
                if (brick_zi != 0) {
                    auto neighbor_brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + (brick_zi - 1) * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                    auto &neighbor_brick_metadata = get_brick_metadata(chunk, neighbor_brick_index);
                    brick_metadata.exposed_nz = neighbor_brick_metadata.has_air_pz;
                    neighbor_bitmask_nz = &chunk->voxel_brick_bitmasks[neighbor_brick_index];
                } else if (chunk_zi != 0) {
                    int32_t neighbor_chunk_index = chunk_xi + chunk_yi * CHUNK_NX + (chunk_zi - 1) * CHUNK_NX * CHUNK_NY + level * CHUNK_NX * CHUNK_NY * CHUNK_NZ;
                    auto &neighbor_chunk = self->chunks[neighbor_chunk_index];
                    if (neighbor_chunk) {
                        auto neighbor_brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + (BRICK_CHUNK_SIZE - 1) * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_chunk, neighbor_brick_index);
                        brick_metadata.exposed_nz = neighbor_brick_metadata.has_air_pz;
                        neighbor_bitmask_nz = &neighbor_chunk->voxel_brick_bitmasks[neighbor_brick_index];
                    } else {
                        brick_metadata.exposed_nz = true;
                    }
                }
                if (brick_xi != BRICK_CHUNK_SIZE - 1) {
                    auto neighbor_brick_index = (brick_xi + 1) + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                    auto &neighbor_brick_metadata = get_brick_metadata(chunk, neighbor_brick_index);
                    brick_metadata.exposed_px = neighbor_brick_metadata.has_air_nx;
                    neighbor_bitmask_px = &chunk->voxel_brick_bitmasks[neighbor_brick_index];
                } else if (chunk_xi != CHUNK_NX - 1) {
                    int32_t neighbor_chunk_index = (chunk_xi + 1) + chunk_yi * CHUNK_NX + chunk_zi * CHUNK_NX * CHUNK_NY + level * CHUNK_NX * CHUNK_NY * CHUNK_NZ;
                    auto &neighbor_chunk = self->chunks[neighbor_chunk_index];
                    if (neighbor_chunk) {
                        auto neighbor_brick_index = 0 + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_chunk, neighbor_brick_index);
                        brick_metadata.exposed_px = neighbor_brick_metadata.has_air_nx;
                        neighbor_bitmask_px = &neighbor_chunk->voxel_brick_bitmasks[neighbor_brick_index];
                    } else {
                        brick_metadata.exposed_px = true;
                    }
                }
                if (brick_yi != BRICK_CHUNK_SIZE - 1) {
                    auto neighbor_brick_index = brick_xi + (brick_yi + 1) * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                    auto &neighbor_brick_metadata = get_brick_metadata(chunk, neighbor_brick_index);
                    brick_metadata.exposed_py = neighbor_brick_metadata.has_air_ny;
                    neighbor_bitmask_py = &chunk->voxel_brick_bitmasks[neighbor_brick_index];
                } else if (chunk_yi != CHUNK_NY - 1) {
                    int32_t neighbor_chunk_index = chunk_xi + (chunk_yi + 1) * CHUNK_NX + chunk_zi * CHUNK_NX * CHUNK_NY + level * CHUNK_NX * CHUNK_NY * CHUNK_NZ;
                    auto &neighbor_chunk = self->chunks[neighbor_chunk_index];
                    if (neighbor_chunk) {
                        auto neighbor_brick_index = brick_xi + 0 * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_chunk, neighbor_brick_index);
                        brick_metadata.exposed_py = neighbor_brick_metadata.has_air_ny;
                        neighbor_bitmask_py = &neighbor_chunk->voxel_brick_bitmasks[neighbor_brick_index];
                    } else {
                        brick_metadata.exposed_py = true;
                    }
                }
                if (brick_zi != BRICK_CHUNK_SIZE - 1) {
                    auto neighbor_brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + (brick_zi + 1) * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                    auto &neighbor_brick_metadata = get_brick_metadata(chunk, neighbor_brick_index);
                    brick_metadata.exposed_pz = neighbor_brick_metadata.has_air_nz;
                    neighbor_bitmask_pz = &chunk->voxel_brick_bitmasks[neighbor_brick_index];
                } else if (chunk_zi != CHUNK_NZ - 1) {
                    int32_t neighbor_chunk_index = chunk_xi + chunk_yi * CHUNK_NX + (chunk_zi + 1) * CHUNK_NX * CHUNK_NY + level * CHUNK_NX * CHUNK_NY * CHUNK_NZ;
                    auto &neighbor_chunk = self->chunks[neighbor_chunk_index];
                    if (neighbor_chunk) {
                        auto neighbor_brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + 0 * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_chunk, neighbor_brick_index);
                        brick_metadata.exposed_pz = neighbor_brick_metadata.has_air_nz;
                        neighbor_bitmask_pz = &neighbor_chunk->voxel_brick_bitmasks[neighbor_brick_index];
                    } else {
                        brick_metadata.exposed_pz = true;
                    }
                }

                bool exposed = brick_metadata.exposed_nx || brick_metadata.exposed_px || brick_metadata.exposed_ny || brick_metadata.exposed_py || brick_metadata.exposed_nz || brick_metadata.exposed_pz;

                auto position = glm::ivec4{brick_xi, brick_yi, brick_zi, -4 + level};
                if (brick_metadata.has_voxel && exposed) {
                    // generate surface brick data
                    auto &attrib_brick = chunk->voxel_brick_attribs[brick_index];
                    attrib_brick = std::make_unique<VoxelAttribBrick>();

                    for (uint32_t zi = 0; zi < VOXEL_BRICK_SIZE; ++zi) {
                        for (uint32_t yi = 0; yi < VOXEL_BRICK_SIZE; ++yi) {
                            for (uint32_t xi = 0; xi < VOXEL_BRICK_SIZE; ++xi) {
                                uint32_t voxel_index = xi + yi * VOXEL_BRICK_SIZE + zi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
                                float x = (float((xi + brick_xi * VOXEL_BRICK_SIZE + chunk_xi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f;
                                float y = (float((yi + brick_yi * VOXEL_BRICK_SIZE + chunk_yi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f;
                                float z = (float((zi + brick_zi * VOXEL_BRICK_SIZE + chunk_zi * VOXEL_CHUNK_SIZE) << level) + 0.5f) / 16.0f;
                                auto dn = voxel_nrm(glm::vec3(x, y, z));
                                auto col = glm::vec3(0.0f);
                                if (dot(dn.nrm, vec3(0, 0, -1)) > 0.5f && dn.val > -0.5f) {
                                    col = glm::vec3(12, 163, 7) / 255.0f;
                                } else {
                                    col = glm::vec3(112, 62, 30) / 255.0f;
                                }
                                attrib_brick->packed_voxels[voxel_index] = pack_voxel(Voxel(std::bit_cast<daxa_f32vec3>(col), std::bit_cast<daxa_f32vec3>(dn.nrm)));
                            }
                        }
                    }

                    auto get_brick_bit = [](VoxelBrickBitmask const &bitmask, uint32_t xi, uint32_t yi, uint32_t zi) {
                        uint32_t voxel_index = xi + yi * VOXEL_BRICK_SIZE + zi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
                        uint32_t voxel_word_index = voxel_index / 32;
                        uint32_t voxel_in_word_index = voxel_index % 32;
                        return (bitmask.bits[voxel_word_index] >> voxel_in_word_index) & 1;
                    };
                    auto set_brick_neighbor_bit = [](VoxelBrickBitmask &bitmask, uint32_t xi, uint32_t yi, uint32_t fi, uint32_t value) {
                        uint32_t voxel_index = xi + yi * VOXEL_BRICK_SIZE + fi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
                        uint32_t voxel_word_index = voxel_index / 32;
                        uint32_t voxel_in_word_index = voxel_index % 32;
                        if (value != 0) {
                            bitmask.neighbor_bits[voxel_word_index] |= (1 << voxel_in_word_index);
                        } else {
                            // bitmask.neighbor_bits[voxel_word_index] &= ~(1 << voxel_in_word_index);
                        }
                    };

                    for (auto &word : bitmask.neighbor_bits) {
                        word = {};
                    }

                    if (neighbor_bitmask_nx != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 0, get_brick_bit(*neighbor_bitmask_nx, VOXEL_BRICK_SIZE - 1, ai, bi));
                            }
                        }
                    }
                    if (neighbor_bitmask_px != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 3, get_brick_bit(*neighbor_bitmask_px, 0, ai, bi));
                            }
                        }
                    }

                    if (neighbor_bitmask_ny != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 1, get_brick_bit(*neighbor_bitmask_ny, ai, VOXEL_BRICK_SIZE - 1, bi));
                            }
                        }
                    }
                    if (neighbor_bitmask_py != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 4, get_brick_bit(*neighbor_bitmask_py, ai, 0, bi));
                            }
                        }
                    }

                    if (neighbor_bitmask_nz != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 2, get_brick_bit(*neighbor_bitmask_nz, ai, bi, VOXEL_BRICK_SIZE - 1));
                            }
                        }
                    }
                    if (neighbor_bitmask_pz != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 5, get_brick_bit(*neighbor_bitmask_pz, ai, bi, 0));
                            }
                        }
                    }

                    chunk->surface_brick_bitmasks.push_back(bitmask);
                    chunk->surface_brick_positions.push_back(position);
                    chunk->surface_attrib_bricks.push_back(*attrib_brick);
                }
            }
        }
    }

    if (update) {
        chunk->bricks_changed = true;
    }
}

void voxel_world::init(VoxelWorld &self) {
    self = new State{};
    self->prev_time = 0.0f;

    std::vector<std::thread> threads;
    threads.reserve(CHUNK_NX * CHUNK_NY * CHUNK_NZ * CHUNK_LEVELS);

    for (int32_t level_i = 0; level_i < CHUNK_LEVELS; ++level_i) {
        for (int32_t chunk_zi = 0; chunk_zi < CHUNK_NZ; ++chunk_zi) {
            for (int32_t chunk_yi = 0; chunk_yi < CHUNK_NY; ++chunk_yi) {
                for (int32_t chunk_xi = 0; chunk_xi < CHUNK_NX; ++chunk_xi) {
                    threads.emplace_back([=]() { generate_chunk(self, chunk_xi, chunk_yi, chunk_zi, level_i); });
                }
            }
        }
    }

    for (auto &thread : threads) {
        thread.join();
    }
    threads.clear();

    for (int32_t level_i = 0; level_i < CHUNK_LEVELS; ++level_i) {
        for (int32_t chunk_zi = 0; chunk_zi < CHUNK_NZ; ++chunk_zi) {
            for (int32_t chunk_yi = 0; chunk_yi < CHUNK_NY; ++chunk_yi) {
                for (int32_t chunk_xi = 0; chunk_xi < CHUNK_NX; ++chunk_xi) {
                    threads.emplace_back([=]() { generate_chunk2(self, chunk_xi, chunk_yi, chunk_zi, level_i); });
                }
            }
        }
    }
    for (auto &thread : threads) {
        thread.join();
    }
}
void voxel_world::deinit(VoxelWorld self) {
    delete self;
}

void voxel_world::update(VoxelWorld self) {
    for (uint32_t chunk_index = 0; chunk_index < MAX_CHUNK_COUNT; ++chunk_index) {
        auto &chunk = self->chunks[chunk_index];
        if (!chunk) {
            continue;
        }

        auto brick_count = chunk->surface_brick_bitmasks.size();

        if (chunk->bricks_changed) {
            if (chunk->render_chunk == nullptr) {
                renderer::init(chunk->render_chunk);
            }
            renderer::update(chunk->render_chunk, brick_count, chunk->surface_brick_bitmasks.data(), chunk->surface_attrib_bricks.data(), (int const *)chunk->surface_brick_positions.data());
            chunk->bricks_changed = false;
        }

        if (brick_count > 0) {
            renderer::render_chunk(chunk->render_chunk, (float const *)&chunk->pos);
        }
    }
}

#define HANDLE_RES(x, message) \
    if ((x) != GVOX_SUCCESS) { \
        return;                \
    }

void voxel_world::load_model(char const *path) {
    auto *file_input = GvoxInputStream{};
    {
        auto file = std::ifstream{path, std::ios::binary};
        auto size = std::filesystem::file_size(path);
        auto bytes = std::vector<uint8_t>(size);
        file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size));
        auto config = GvoxByteBufferInputStreamConfig{.data = bytes.data(), .size = bytes.size()};
        auto input_ci = GvoxInputStreamCreateInfo{};
        input_ci.struct_type = GVOX_STRUCT_TYPE_INPUT_STREAM_CREATE_INFO;
        input_ci.next = nullptr;
        input_ci.cb_args.config = &config;
        input_ci.description = gvox_input_stream_byte_buffer_description();
        HANDLE_RES(gvox_create_input_stream(&input_ci, &file_input), "Failed to create (byte buffer) input stream");
    }

    auto *file_parser = GvoxParser{};
    {
        auto parser_collection = GvoxParserDescriptionCollection{
            .struct_type = GVOX_STRUCT_TYPE_PARSER_DESCRIPTION_COLLECTION,
            .next = nullptr,
        };
        gvox_enumerate_standard_parser_descriptions(&parser_collection.descriptions, &parser_collection.description_n);
        HANDLE_RES(gvox_create_parser_from_input(&parser_collection, file_input, &file_parser), "Failed to create parser");
    }

    auto *input_iterator = GvoxIterator{};
    {
        auto parse_iter_ci = GvoxParseIteratorCreateInfo{
            .struct_type = GVOX_STRUCT_TYPE_PARSE_ITERATOR_CREATE_INFO,
            .next = nullptr,
            .parser = file_parser,
        };
        auto iter_ci = GvoxIteratorCreateInfo{
            .struct_type = GVOX_STRUCT_TYPE_ITERATOR_CREATE_INFO,
            .next = &parse_iter_ci,
        };
        gvox_create_iterator(&iter_ci, &input_iterator);
    }

    auto iter_value = GvoxIteratorValue{};
    auto advance_info = GvoxIteratorAdvanceInfo{
        .input_stream = file_input,
        .mode = GVOX_ITERATOR_ADVANCE_MODE_NEXT,
    };

    // auto voxel_count = size_t{};
    // using Clock = std::chrono::high_resolution_clock;
    // auto t0 = Clock::now();
    // while (true) {
    //     gvox_iterator_advance(input_iterator, &advance_info, &iter_value);
    //     if (iter_value.tag == GVOX_ITERATOR_VALUE_TYPE_NULL) {
    //         break;
    //     }
    //     if (iter_value.tag == GVOX_ITERATOR_VALUE_TYPE_NODE_BEGIN) {
    //         // there has been a chunk update. Insert it into the set of dirty chunks
    //         auto range_axis_n = std::min<uint32_t>(iter_value.range.offset.axis_n, 3);
    //         int64_t chunk_min[3] = {0, 0, 0};
    //         int64_t chunk_max[3] = {CHUNK_NX, CHUNK_NY, CHUNK_NZ};
    //         for (uint32_t axis_i = 0; axis_i < range_axis_n; ++axis_i) {
    //             chunk_min[axis_i] = std::max(iter_value.range.offset.axis[axis_i] / CHUNK_SIZE, chunk_min[axis_i]);
    //             chunk_max[axis_i] = std::min((iter_value.range.offset.axis[axis_i] + int64_t(iter_value.range.extent.axis[axis_i]) + (CHUNK_SIZE - 1)) / CHUNK_SIZE, chunk_max[axis_i]);
    //         }
    //         for (int64_t chunk_zi = chunk_min[2]; chunk_zi < chunk_max[2]; ++chunk_zi) {
    //             for (int64_t chunk_yi = chunk_min[1]; chunk_yi < chunk_max[1]; ++chunk_yi) {
    //                 for (int64_t chunk_xi = chunk_min[0]; chunk_xi < chunk_max[0]; ++chunk_xi) {
    //                     uint32_t chunk_index = chunk_xi + chunk_yi * CHUNK_NX + chunk_zi * CHUNK_NX * CHUNK_NY;
    //                     chunk_updates.insert(chunk_index);
    //                 }
    //             }
    //         }
    //     }
    //     if (iter_value.tag == GVOX_ITERATOR_VALUE_TYPE_LEAF) {
    //         // And for every "Leaf" node in the model, we'll write it into our container.
    //         auto fill_info = GvoxFillInfo{
    //             .struct_type = GVOX_STRUCT_TYPE_FILL_INFO,
    //             .next = nullptr,
    //             .src_data = iter_value.voxel_data,
    //             .src_desc = iter_value.voxel_desc,
    //             .dst = raw_container,
    //             .range = iter_value.range,
    //         };
    //         HANDLE_RES(gvox_fill(&fill_info), "Failed to do fill");
    //         ++voxel_count;
    //     }
    // }
    // auto t1 = Clock::now();
    // debug_utils::Console::add_log(fmt::format("{} voxels loaded in {} seconds", voxel_count, std::chrono::duration<float>(t1 - t0).count()));

    gvox_destroy_iterator(input_iterator);
    gvox_destroy_input_stream(file_input);
    gvox_destroy_parser(file_parser);
}
