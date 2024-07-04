#include "voxel_world.hpp"
#include "voxels/defs.inl"
#include "voxels/voxel_mesh.inl"

#include <chrono>
#include <glm/common.hpp>
#include <glm/glm.hpp>

#include <gvox/gvox.h>
#include <gvox/streams/input/byte_buffer.h>
#include <gvox/containers/raw.h>

#include <renderer/renderer.hpp>
#include <utilities/thread_pool.hpp>
#include <utilities/ispc_instrument.hpp>
#include <utilities/debug.hpp>

#include <fmt/format.h>

#include <fstream>
#include <array>
#include <vector>
#include <thread>

#include "generation/generation.hpp"

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

enum GenerationStage {
    NOT_GENERATED,
    GENERATED_BITMASK,
    GENERATED_SURFACE_BRICK_ATTRIBS,
};

struct VoxelSimAttribBrick {
    float densities[VOXELS_PER_BRICK];
};

struct Chunk {
    std::array<VoxelBrickBitmask, BRICKS_PER_CHUNK> voxel_brick_bitmasks{};
    std::array<std::unique_ptr<VoxelRenderAttribBrick>, BRICKS_PER_CHUNK> voxel_brick_render_attribs{};
    std::array<std::unique_ptr<VoxelSimAttribBrick>, BRICKS_PER_CHUNK> voxel_brick_sim_attribs{};
    std::array<glm::ivec4, BRICKS_PER_CHUNK> voxel_brick_positions{};
    std::vector<int> surface_brick_indices;

    renderer::Chunk render_chunk = nullptr;
    glm::vec3 pos;
    bool bricks_changed;

    Chunk() {
        render_chunk = create_chunk(g_renderer);
    }
    ~Chunk() {
        if (render_chunk != nullptr) {
            destroy_chunk(g_renderer, render_chunk);
        }
    }
};

using Clock = std::chrono::steady_clock;

struct voxel_world::State {
    std::array<std::unique_ptr<Chunk>, MAX_CHUNK_COUNT> chunks;
    Clock::time_point start_time;
    Clock::time_point prev_time;

    std::thread test_chunk_thread;
    bool launch_update;

    std::atomic_uint64_t generate_chunk1s_total;
    std::atomic_uint64_t generate_chunk2s_total;

    std::atomic_uint64_t generate_chunk1s_total_n;
    std::atomic_uint64_t generate_chunk2s_total_n;
};

struct DensityNrm {
    float val;
    glm::vec3 nrm;
};

#include <random>

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

constexpr int32_t CHUNK_NX = 16;
constexpr int32_t CHUNK_NY = 16;
constexpr int32_t CHUNK_NZ = 16;
constexpr int32_t CHUNK_LEVELS = 1;

static_assert(CHUNK_NX * CHUNK_NY * CHUNK_NZ * CHUNK_LEVELS <= MAX_CHUNK_COUNT);

NoiseSettings noise_settings{
    .persistence = 0.20f,
    .lacunarity = 4.0f,
    .scale = 0.05f,
    .amplitude = 20.0f,
    .octaves = 5,
};

auto get_brick_metadata(std::unique_ptr<Chunk> &chunk, auto brick_index) -> BrickMetadata & {
    return *reinterpret_cast<BrickMetadata *>(&chunk->voxel_brick_bitmasks[brick_index].metadata);
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
        auto p1 = p0 + (BRICK_CHUNK_SIZE * VOXEL_BRICK_SIZE << level) / 16.0f;
        auto minmax = voxel_minmax_value_cpp(&noise_settings, RANDOM_VALUES.data(), p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);
        if (minmax.min >= 0.0f || minmax.max < 0.0f) {
            // uniform
            if (minmax.min < 0.0f) {
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

    auto t0 = Clock::now();

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
                    auto p1 = p0 + (VOXEL_BRICK_SIZE << level) / 16.0f;
                    auto minmax = voxel_minmax_value_cpp(&noise_settings, RANDOM_VALUES.data(), p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);
                    if (minmax.min >= 0.0f || minmax.max < 0.0f) {
                        // uniform
                        if (minmax.min < 0.0f) {
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

                self->generate_chunk1s_total_n += 1;
                generate_bitmask(brick_xi, brick_yi, brick_zi, chunk_xi, chunk_yi, chunk_zi, level, bitmask.bits, &bitmask.metadata, &noise_settings, RANDOM_VALUES.data());
            }
        }
    }

    auto t1 = Clock::now();

    self->generate_chunk1s_total += (t1 - t0).count();
}

auto generate_chunk2(voxel_world::VoxelWorld self, int32_t chunk_xi, int32_t chunk_yi, int32_t chunk_zi, int32_t level, bool update = true) {
    int32_t chunk_index = chunk_xi + chunk_yi * CHUNK_NX + chunk_zi * CHUNK_NX * CHUNK_NY + level * CHUNK_NX * CHUNK_NY * CHUNK_NZ;
    auto &chunk = self->chunks[chunk_index];
    if (!chunk) {
        return;
    }

    auto t0 = Clock::now();

    chunk->surface_brick_indices.clear();

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
                        // brick_metadata.exposed_nx = true;
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
                        // brick_metadata.exposed_ny = true;
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
                        // brick_metadata.exposed_nz = true;
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
                        // brick_metadata.exposed_px = true;
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
                        // brick_metadata.exposed_py = true;
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
                        // brick_metadata.exposed_pz = true;
                    }
                }

                bool exposed = brick_metadata.exposed_nx || brick_metadata.exposed_px || brick_metadata.exposed_ny || brick_metadata.exposed_py || brick_metadata.exposed_nz || brick_metadata.exposed_pz;

                auto position = glm::ivec4{brick_xi, brick_yi, brick_zi, -4 + level};
                if (brick_metadata.has_voxel && exposed) {
                    // generate surface brick data
                    auto &render_attrib_brick = chunk->voxel_brick_render_attribs[brick_index];
                    auto &sim_attrib_brick = chunk->voxel_brick_sim_attribs[brick_index];
                    self->generate_chunk2s_total_n += 1;

                    if (render_attrib_brick == nullptr) {
                        render_attrib_brick = std::make_unique<VoxelRenderAttribBrick>();
                        sim_attrib_brick = std::make_unique<VoxelSimAttribBrick>();
                        generate_attributes(brick_xi, brick_yi, brick_zi, chunk_xi, chunk_yi, chunk_zi, level, (uint32_t *)render_attrib_brick->packed_voxels, (float *)sim_attrib_brick->densities, &noise_settings, RANDOM_VALUES.data());
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
                    } else {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 0, 1);
                            }
                        }
                    }
                    if (neighbor_bitmask_px != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 3, get_brick_bit(*neighbor_bitmask_px, 0, ai, bi));
                            }
                        }
                    } else {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 3, 1);
                            }
                        }
                    }

                    if (neighbor_bitmask_ny != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 1, get_brick_bit(*neighbor_bitmask_ny, ai, VOXEL_BRICK_SIZE - 1, bi));
                            }
                        }
                    } else {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 1, 1);
                            }
                        }
                    }
                    if (neighbor_bitmask_py != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 4, get_brick_bit(*neighbor_bitmask_py, ai, 0, bi));
                            }
                        }
                    } else {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 4, 1);
                            }
                        }
                    }

                    if (neighbor_bitmask_nz != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 2, get_brick_bit(*neighbor_bitmask_nz, ai, bi, VOXEL_BRICK_SIZE - 1));
                            }
                        }
                    } else {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 2, 1);
                            }
                        }
                    }
                    if (neighbor_bitmask_pz != nullptr) {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 5, get_brick_bit(*neighbor_bitmask_pz, ai, bi, 0));
                            }
                        }
                    } else {
                        for (uint32_t bi = 0; bi < VOXEL_BRICK_SIZE; ++bi) {
                            for (uint32_t ai = 0; ai < VOXEL_BRICK_SIZE; ++ai) {
                                set_brick_neighbor_bit(bitmask, ai, bi, 5, 1);
                            }
                        }
                    }

                    chunk->voxel_brick_positions[brick_index] = position;
                    chunk->surface_brick_indices.push_back(brick_index);
                }
            }
        }
    }

    auto t1 = Clock::now();
    self->generate_chunk2s_total += (t1 - t0).count();

    if (update) {
        chunk->bricks_changed = true;
    }
}

auto generate_all_chunks(voxel_world::VoxelWorld self) {
    std::vector<std::pair<thread_pool::Task, void *>> tasks;
    tasks.reserve(CHUNK_NX * CHUNK_NY * CHUNK_NZ * CHUNK_LEVELS);

    self->generate_chunk1s_total = {};
    self->generate_chunk2s_total = {};
    auto generate_chunk1s_main_total_ns = uint64_t{};
    auto generate_chunk2s_main_total_ns = uint64_t{};

    struct GenChunkArgs {
        voxel_world::VoxelWorld self;
        int32_t chunk_xi;
        int32_t chunk_yi;
        int32_t chunk_zi;
        int32_t level;
        bool update = true;
    };

    {
        auto t0 = Clock::now();

        for (int32_t level_i = 0; level_i < CHUNK_LEVELS; ++level_i) {
            for (int32_t chunk_zi = 0; chunk_zi < CHUNK_NZ; ++chunk_zi) {
                for (int32_t chunk_yi = 0; chunk_yi < CHUNK_NY; ++chunk_yi) {
                    for (int32_t chunk_xi = 0; chunk_xi < CHUNK_NX; ++chunk_xi) {
                        auto *user_ptr = new GenChunkArgs{self, chunk_xi, chunk_yi, chunk_zi, level_i};
                        auto task = thread_pool::create_task([](void *user_ptr) { auto const &args = *(GenChunkArgs*)user_ptr; generate_chunk(args.self, args.chunk_xi, args.chunk_yi, args.chunk_zi, args.level); }, user_ptr);
                        thread_pool::async_dispatch(task);
                        tasks.emplace_back(task, user_ptr);
                    }
                }
            }
        }

        for (auto &[task, user_ptr] : tasks) {
            thread_pool::wait(task);
            thread_pool::destroy_task(task);
            delete (GenChunkArgs *)user_ptr;
        }
        tasks.clear();

        auto t1 = Clock::now();
        generate_chunk1s_main_total_ns += (t1 - t0).count();
    }

    {
        auto t0 = Clock::now();
        for (int32_t level_i = 0; level_i < CHUNK_LEVELS; ++level_i) {
            for (int32_t chunk_zi = 0; chunk_zi < CHUNK_NZ; ++chunk_zi) {
                for (int32_t chunk_yi = 0; chunk_yi < CHUNK_NY; ++chunk_yi) {
                    for (int32_t chunk_xi = 0; chunk_xi < CHUNK_NX; ++chunk_xi) {
                        auto *user_ptr = new GenChunkArgs{self, chunk_xi, chunk_yi, chunk_zi, level_i};
                        auto task = thread_pool::create_task([](void *user_ptr) { auto const &args = *(GenChunkArgs*)user_ptr; generate_chunk2(args.self, args.chunk_xi, args.chunk_yi, args.chunk_zi, args.level); }, user_ptr);
                        thread_pool::async_dispatch(task);
                        tasks.emplace_back(task, user_ptr);
                    }
                }
            }
        }
        for (auto &[task, user_ptr] : tasks) {
            thread_pool::wait(task);
            thread_pool::destroy_task(task);
            delete (GenChunkArgs *)user_ptr;
        }
        tasks.clear();

        auto t1 = Clock::now();
        generate_chunk2s_main_total_ns += (t1 - t0).count();
    }

    auto generate_chunk1s_total = std::chrono::duration<float, std::micro>(std::chrono::duration<uint64_t, std::nano>(self->generate_chunk1s_total)).count();
    auto generate_chunk2s_total = std::chrono::duration<float, std::micro>(std::chrono::duration<uint64_t, std::nano>(self->generate_chunk2s_total)).count();

    auto generate_chunk1s_main_total = std::chrono::duration<float, std::micro>(std::chrono::duration<uint64_t, std::nano>(generate_chunk1s_main_total_ns)).count();
    auto generate_chunk2s_main_total = std::chrono::duration<float, std::micro>(std::chrono::duration<uint64_t, std::nano>(generate_chunk2s_main_total_ns)).count();

    add_log(g_console, fmt::format("1: {} s | {} us/brick ({} total bricks) {} us/brick per thread",
                                   generate_chunk1s_main_total / 1'000'000,
                                   generate_chunk1s_main_total / self->generate_chunk1s_total_n,
                                   self->generate_chunk1s_total_n.load(),
                                   generate_chunk1s_total / self->generate_chunk1s_total_n)
                           .c_str());
    add_log(g_console, fmt::format("2: {} s | {} us/brick ({} total bricks) {} us/brick per thread",
                                   generate_chunk2s_main_total / 1'000'000,
                                   generate_chunk2s_main_total / self->generate_chunk2s_total_n,
                                   self->generate_chunk2s_total_n.load(),
                                   generate_chunk2s_total / self->generate_chunk2s_total_n)
                           .c_str());

    ISPCPrintInstrument();
}

using namespace glm;

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct GlmAabb {
    vec3 minimum;
    vec3 maximum;
};

float hitAabb(const GlmAabb aabb, const Ray r) {
    if (all(greaterThanEqual(r.origin, aabb.minimum)) && all(lessThanEqual(r.origin, aabb.maximum))) {
        return 0.0f;
    }
    vec3 invDir = 1.0f / r.direction;
    vec3 tbot = invDir * (aabb.minimum - r.origin);
    vec3 ttop = invDir * (aabb.maximum - r.origin);
    vec3 tmin = min(ttop, tbot);
    vec3 tmax = max(ttop, tbot);
    float t0 = max(tmin.x, max(tmin.y, tmin.z));
    float t1 = min(tmax.x, min(tmax.y, tmax.z));
    return t1 > max(t0, 0.0f) ? t0 : -1.0f;
}

constexpr auto positive_mod(auto x, auto d) {
    return ((x % d) + d) % d;
}

auto get_voxel_is_solid(voxel_world::VoxelWorld self, ivec3 p) -> bool {
    ivec3 chunk_i = p / int(VOXEL_CHUNK_SIZE);

    if (any(lessThan(chunk_i, ivec3(0))) || any(greaterThanEqual(chunk_i, ivec3(CHUNK_NX, CHUNK_NY, CHUNK_NZ)))) {
        return false;
    }

    ivec3 brick_i = positive_mod(p / int(VOXEL_BRICK_SIZE), int(BRICK_CHUNK_SIZE));
    ivec3 voxel_i = positive_mod(p, int(VOXEL_BRICK_SIZE));
    auto chunk_index = chunk_i.x + chunk_i.y * CHUNK_NX + chunk_i.z * CHUNK_NX * CHUNK_NY;
    auto brick_index = brick_i.x + brick_i.y * BRICK_CHUNK_SIZE + brick_i.z * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
    auto voxel_index = voxel_i.x + voxel_i.y * VOXEL_BRICK_SIZE + voxel_i.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    auto &chunk = self->chunks[chunk_index];
    if (!chunk) {
        return false;
    }
    auto &brick_bitmask = chunk->voxel_brick_bitmasks[brick_index];
    uint voxel_word_index = voxel_index / 32;
    uint voxel_in_word_index = voxel_index % 32;
    return ((brick_bitmask.bits[voxel_word_index] >> voxel_in_word_index) & 1) != 0;
}

void set_voxel_bit(voxel_world::VoxelWorld self, ivec3 p, bool value) {
    ivec3 chunk_i = p / int(VOXEL_CHUNK_SIZE);

    if (any(lessThan(chunk_i, ivec3(0))) || any(greaterThanEqual(chunk_i, ivec3(CHUNK_NX, CHUNK_NY, CHUNK_NZ)))) {
        return;
    }

    ivec3 brick_i = positive_mod(p / int(VOXEL_BRICK_SIZE), int(BRICK_CHUNK_SIZE));
    ivec3 voxel_i = positive_mod(p, int(VOXEL_BRICK_SIZE));
    auto chunk_index = chunk_i.x + chunk_i.y * CHUNK_NX + chunk_i.z * CHUNK_NX * CHUNK_NY;
    auto brick_index = brick_i.x + brick_i.y * BRICK_CHUNK_SIZE + brick_i.z * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
    auto voxel_index = voxel_i.x + voxel_i.y * VOXEL_BRICK_SIZE + voxel_i.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;

    auto &chunk = self->chunks[chunk_index];
    if (!chunk) {
        chunk = std::make_unique<Chunk>();
        chunk->pos = chunk_i;
    }
    auto &brick_bitmask = chunk->voxel_brick_bitmasks[brick_index];
    uint voxel_word_index = voxel_index / 32;
    uint voxel_in_word_index = voxel_index % 32;

    bool prev_value = ((brick_bitmask.bits[voxel_word_index] >> voxel_in_word_index) & 1) != 0;

    auto &brick_metadata = get_brick_metadata(chunk, brick_index);

    if (prev_value != value) {
        chunk->bricks_changed = true;

        auto notify_neighbor_chunk = [self](glm::ivec3 n_chunk_i) {
            if (any(lessThan(n_chunk_i, ivec3(0))) || any(greaterThanEqual(n_chunk_i, ivec3(CHUNK_NX, CHUNK_NY, CHUNK_NZ)))) {
                return;
            }
            auto n_chunk_index = n_chunk_i.x + n_chunk_i.y * CHUNK_NX + n_chunk_i.z * CHUNK_NX * CHUNK_NY;
            auto &n_chunk = self->chunks[n_chunk_index];
            if (!n_chunk) {
                return;
            }
            n_chunk->bricks_changed = true;
        };

        if (voxel_i.x == 0 && brick_i.x == 0) {
            notify_neighbor_chunk(chunk_i - glm::ivec3(1, 0, 0));
        } else if (voxel_i.x == VOXEL_BRICK_SIZE - 1 && brick_i.x == BRICK_CHUNK_SIZE - 1) {
            notify_neighbor_chunk(chunk_i + glm::ivec3(1, 0, 0));
        }
        if (voxel_i.y == 0 && brick_i.y == 0) {
            notify_neighbor_chunk(chunk_i - glm::ivec3(0, 1, 0));
        } else if (voxel_i.y == VOXEL_BRICK_SIZE - 1 && brick_i.y == BRICK_CHUNK_SIZE - 1) {
            notify_neighbor_chunk(chunk_i + glm::ivec3(0, 1, 0));
        }
        if (voxel_i.z == 0 && brick_i.z == 0) {
            notify_neighbor_chunk(chunk_i - glm::ivec3(0, 0, 1));
        } else if (voxel_i.z == VOXEL_BRICK_SIZE - 1 && brick_i.z == BRICK_CHUNK_SIZE - 1) {
            notify_neighbor_chunk(chunk_i + glm::ivec3(0, 0, 1));
        }
    }

    if (value) {
        brick_bitmask.bits[voxel_word_index] |= 1 << voxel_in_word_index;
        brick_metadata.has_voxel = true;
        auto &render_attrib_brick = chunk->voxel_brick_render_attribs[brick_index];
        auto &sim_attrib_brick = chunk->voxel_brick_sim_attribs[brick_index];
        if (!render_attrib_brick) {
            render_attrib_brick = std::make_unique<VoxelRenderAttribBrick>();
            sim_attrib_brick = std::make_unique<VoxelSimAttribBrick>();
            generate_attributes(brick_i.x, brick_i.y, brick_i.z, chunk_i.x, chunk_i.y, chunk_i.z, 0, (uint32_t *)render_attrib_brick->packed_voxels, (float *)sim_attrib_brick->densities, &noise_settings, RANDOM_VALUES.data());
        }
    } else {
        brick_bitmask.bits[voxel_word_index] &= ~(1 << voxel_in_word_index);
        brick_metadata.has_air_px = brick_metadata.has_air_px || voxel_i.x == VOXEL_BRICK_SIZE - 1;
        brick_metadata.has_air_nx = brick_metadata.has_air_nx || voxel_i.x == 0;
        brick_metadata.has_air_py = brick_metadata.has_air_py || voxel_i.y == VOXEL_BRICK_SIZE - 1;
        brick_metadata.has_air_ny = brick_metadata.has_air_ny || voxel_i.y == 0;
        brick_metadata.has_air_pz = brick_metadata.has_air_pz || voxel_i.z == VOXEL_BRICK_SIZE - 1;
        brick_metadata.has_air_nz = brick_metadata.has_air_nz || voxel_i.z == 0;
    }
}

#include "pack_unpack.inl"

void set_voxel_attrib(voxel_world::VoxelWorld self, ivec3 p, Voxel value) {
    ivec3 chunk_i = p / int(VOXEL_CHUNK_SIZE);

    if (any(lessThan(chunk_i, ivec3(0))) || any(greaterThanEqual(chunk_i, ivec3(CHUNK_NX, CHUNK_NY, CHUNK_NZ)))) {
        return;
    }

    ivec3 brick_i = positive_mod(p / int(VOXEL_BRICK_SIZE), int(BRICK_CHUNK_SIZE));
    ivec3 voxel_i = positive_mod(p, int(VOXEL_BRICK_SIZE));
    auto chunk_index = chunk_i.x + chunk_i.y * CHUNK_NX + chunk_i.z * CHUNK_NX * CHUNK_NY;
    auto brick_index = brick_i.x + brick_i.y * BRICK_CHUNK_SIZE + brick_i.z * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
    auto voxel_index = voxel_i.x + voxel_i.y * VOXEL_BRICK_SIZE + voxel_i.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    auto &chunk = self->chunks[chunk_index];
    if (!chunk) {
        chunk = std::make_unique<Chunk>();
        chunk->pos = chunk_i;
    }
    auto &render_attrib_brick = chunk->voxel_brick_render_attribs[brick_index];
    if (!render_attrib_brick) {
        auto &sim_attrib_brick = chunk->voxel_brick_sim_attribs[brick_index];
        render_attrib_brick = std::make_unique<VoxelRenderAttribBrick>();
        sim_attrib_brick = std::make_unique<VoxelSimAttribBrick>();
        generate_attributes(brick_i.x, brick_i.y, brick_i.z, chunk_i.x, chunk_i.y, chunk_i.z, 0, (uint32_t *)render_attrib_brick->packed_voxels, (float *)sim_attrib_brick->densities, &noise_settings, RANDOM_VALUES.data());
    }

    PackedVoxel prev_value = render_attrib_brick->packed_voxels[voxel_index];
    PackedVoxel new_value = pack_voxel(value);

    if (prev_value.data != new_value.data) {
        chunk->bricks_changed = true;
    }

    render_attrib_brick->packed_voxels[voxel_index] = new_value;
}

void set_voxel_sim_attrib(voxel_world::VoxelWorld self, ivec3 p, float density) {
    ivec3 chunk_i = p / int(VOXEL_CHUNK_SIZE);

    if (any(lessThan(chunk_i, ivec3(0))) || any(greaterThanEqual(chunk_i, ivec3(CHUNK_NX, CHUNK_NY, CHUNK_NZ)))) {
        return;
    }

    ivec3 brick_i = positive_mod(p / int(VOXEL_BRICK_SIZE), int(BRICK_CHUNK_SIZE));
    ivec3 voxel_i = positive_mod(p, int(VOXEL_BRICK_SIZE));
    auto chunk_index = chunk_i.x + chunk_i.y * CHUNK_NX + chunk_i.z * CHUNK_NX * CHUNK_NY;
    auto brick_index = brick_i.x + brick_i.y * BRICK_CHUNK_SIZE + brick_i.z * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
    auto voxel_index = voxel_i.x + voxel_i.y * VOXEL_BRICK_SIZE + voxel_i.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    auto &chunk = self->chunks[chunk_index];
    if (!chunk) {
        chunk = std::make_unique<Chunk>();
        chunk->pos = chunk_i;
    }
    auto &sim_attrib_brick = chunk->voxel_brick_sim_attribs[brick_index];
    if (!sim_attrib_brick) {
        auto &render_attrib_brick = chunk->voxel_brick_render_attribs[brick_index];
        render_attrib_brick = std::make_unique<VoxelRenderAttribBrick>();
        sim_attrib_brick = std::make_unique<VoxelSimAttribBrick>();
        generate_attributes(brick_i.x, brick_i.y, brick_i.z, chunk_i.x, chunk_i.y, chunk_i.z, 0, (uint32_t *)render_attrib_brick->packed_voxels, (float *)sim_attrib_brick->densities, &noise_settings, RANDOM_VALUES.data());
    }

    auto prev_value = sim_attrib_brick->densities[voxel_index];
    auto new_value = density;

    if (prev_value != new_value) {
        chunk->bricks_changed = true;
    }

    sim_attrib_brick->densities[voxel_index] = new_value;
}

auto get_voxel_sim_attrib(voxel_world::VoxelWorld self, ivec3 p, bool generate) -> float {
    ivec3 chunk_i = p / int(VOXEL_CHUNK_SIZE);

    if (any(lessThan(chunk_i, ivec3(0))) || any(greaterThanEqual(chunk_i, ivec3(CHUNK_NX, CHUNK_NY, CHUNK_NZ)))) {
        return 0;
    }

    ivec3 brick_i = positive_mod(p / int(VOXEL_BRICK_SIZE), int(BRICK_CHUNK_SIZE));
    ivec3 voxel_i = positive_mod(p, int(VOXEL_BRICK_SIZE));
    auto chunk_index = chunk_i.x + chunk_i.y * CHUNK_NX + chunk_i.z * CHUNK_NX * CHUNK_NY;
    auto brick_index = brick_i.x + brick_i.y * BRICK_CHUNK_SIZE + brick_i.z * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
    auto voxel_index = voxel_i.x + voxel_i.y * VOXEL_BRICK_SIZE + voxel_i.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    auto &chunk = self->chunks[chunk_index];
    if (!chunk) {
        if (generate) {
            chunk = std::make_unique<Chunk>();
            chunk->pos = chunk_i;
        } else {
            return 0;
        }
    }
    auto &sim_attrib_brick = chunk->voxel_brick_sim_attribs[brick_index];

    if (!sim_attrib_brick) {
        if (generate) {
            auto &render_attrib_brick = chunk->voxel_brick_render_attribs[brick_index];
            render_attrib_brick = std::make_unique<VoxelRenderAttribBrick>();
            sim_attrib_brick = std::make_unique<VoxelSimAttribBrick>();
            generate_attributes(brick_i.x, brick_i.y, brick_i.z, chunk_i.x, chunk_i.y, chunk_i.z, 0, (uint32_t *)render_attrib_brick->packed_voxels, (float *)sim_attrib_brick->densities, &noise_settings, RANDOM_VALUES.data());
        } else {
            return 0;
        }
    }

    return sim_attrib_brick->densities[voxel_index];
}

auto get_voxel_attrib(voxel_world::VoxelWorld self, ivec3 p) -> Voxel {
    ivec3 chunk_i = p / int(VOXEL_CHUNK_SIZE);

    if (any(lessThan(chunk_i, ivec3(0))) || any(greaterThanEqual(chunk_i, ivec3(CHUNK_NX, CHUNK_NY, CHUNK_NZ)))) {
        return {};
    }

    ivec3 brick_i = positive_mod(p / int(VOXEL_BRICK_SIZE), int(BRICK_CHUNK_SIZE));
    ivec3 voxel_i = positive_mod(p, int(VOXEL_BRICK_SIZE));
    auto chunk_index = chunk_i.x + chunk_i.y * CHUNK_NX + chunk_i.z * CHUNK_NX * CHUNK_NY;
    auto brick_index = brick_i.x + brick_i.y * BRICK_CHUNK_SIZE + brick_i.z * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
    auto voxel_index = voxel_i.x + voxel_i.y * VOXEL_BRICK_SIZE + voxel_i.z * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
    auto &chunk = self->chunks[chunk_index];
    if (!chunk) {
        return {};
    }
    auto &brick_attribs = chunk->voxel_brick_render_attribs[brick_index];
    if (!brick_attribs) {
        return {};
    }

    return unpack_voxel(brick_attribs->packed_voxels[voxel_index]);
}

auto dda_voxels(voxel_world::VoxelWorld self, Ray ray, int max_iter, float max_dist) -> std::tuple<ivec3, ivec3, float> {
    using Line = std::array<vec3, 3>;
    using Point = std::array<vec3, 3>;
    using Box = std::array<vec3, 3>;

    float tHit = -1;
    GlmAabb aabb = {{0, 0, 0}, glm::vec3(CHUNK_NX, CHUNK_NY, CHUNK_NZ) * float(VOXEL_CHUNK_SIZE)};
    tHit = hitAabb(aabb, ray);
    const float BIAS = uintBitsToFloat(0x3f800040); // uintBitsToFloat(0x3f800040) == 1.00000762939453125
    ray.origin += ray.direction * tHit * BIAS;

    if (tHit < 0) {
        return {ivec3{0, 0, 0}, ivec3{0, 0, 0}, -1.0f};
    }

    ivec3 bmin = ivec3(floor(aabb.minimum));
    ivec3 mapPos = clamp(ivec3(floor(ray.origin * 16.0f)) - bmin, ivec3(aabb.minimum), ivec3(aabb.maximum));
    vec3 deltaDist = abs(vec3(length(ray.direction)) / ray.direction);
    vec3 sideDist = (sign(ray.direction) * (vec3(mapPos + bmin) - ray.origin * 16.0f) + (sign(ray.direction) * 0.5f) + 0.5f) * deltaDist;
    ivec3 rayStep = ivec3(sign(ray.direction));
    bvec3 mask = lessThanEqual(sideDist, min(vec3(sideDist.y, sideDist.z, sideDist.x), vec3(sideDist.z, sideDist.x, sideDist.y)));

    // vec3 prev_pos = (vec3(mapPos) + 0.5f) / 16.0f;
    const int max_steps = min(max_iter, int(aabb.maximum.x + aabb.maximum.y + aabb.maximum.z));

    for (int i = 0; i < max_steps; i++) {
        if (get_voxel_is_solid(self, mapPos) == true) {
            aabb.minimum += vec3(mapPos) * (1.0f / 16.0f);
            aabb.maximum = aabb.minimum + (1.0f / 16.0f);
            tHit += hitAabb(aabb, ray);

            // int x_face = (mask.x * ((rayStep.x + 1) / 2 + 0));
            // int y_face = (mask.y * ((rayStep.y + 1) / 2 + 2));
            // int z_face = (mask.z * ((rayStep.z + 1) / 2 + 4));
            // x_face + y_face + z_face

            return {mapPos, -rayStep * ivec3(mask), tHit};
        }
        mask = lessThanEqual(sideDist, min(vec3(sideDist.y, sideDist.z, sideDist.x), vec3(sideDist.z, sideDist.x, sideDist.y)));
        sideDist += vec3(mask) * deltaDist;
        mapPos += ivec3(vec3(mask)) * rayStep;
        bool outside_l = any(lessThan(mapPos, ivec3(aabb.minimum)));
        bool outside_g = any(greaterThanEqual(mapPos, ivec3(aabb.maximum)));
        float dist = dot(sideDist, vec3(mask));
        bool past_max_dist = dist > max_dist;
        if ((int(outside_l) | int(outside_g) | int(past_max_dist)) != 0) {
            break;
        }

        // vec3 next_pos = (vec3(mapPos) + 0.5f) / 16.0f;
        // auto line = Line{prev_pos, next_pos, {1.0f, 1.0f, 1.0f}};
        // submit_debug_lines(g_renderer, (renderer::Line const *)&line, 1);
        // auto pt = Point{next_pos, {0.0f, 1.0f, 1.0f}, glm::vec3(0.25f / 16.0f, 0.25f / 16.0f, 1.0f)};
        // submit_debug_points(g_renderer, (renderer::Point const *)&pt, 1);
        // prev_pos = next_pos;
    }

    return {ivec3{0, 0, 0}, {0, 0, 0}, -1.0f};
}

void voxel_world::init(VoxelWorld &self) {
    self = new State{};
    self->start_time = Clock::now();
    self->prev_time = self->start_time;
    generate_all_chunks(self);
}
void voxel_world::deinit(VoxelWorld self) {
    delete self;
}

void voxel_world::update(VoxelWorld self) {
    auto now = Clock::now();
    auto elapsed = std::chrono::duration<float>(now - self->prev_time).count();
    auto time = std::chrono::duration<float>(now - self->start_time).count();

    if (elapsed > 0.25f && false) {
        using namespace std::chrono_literals;
        self->prev_time = now;
        noise_settings.scale = (sin(time) * 0.5f + 0.5f) * 0.1f + 0.25f;
        noise_settings.amplitude = 100.0f;

        for (int i = 0; i < 8; ++i) {
            int xi = (i >> 0) & 1;
            int yi = (i >> 1) & 1;
            int zi = (i >> 2) & 1;
            generate_chunk(self, xi, yi, zi, 0);
        }

        for (int i = 0; i < 8; ++i) {
            int xi = (i >> 0) & 1;
            int yi = (i >> 1) & 1;
            int zi = (i >> 2) & 1;
            generate_chunk2(self, xi, yi, zi, 0);
        }

        noise_settings.scale = 0.05f;
        noise_settings.amplitude = 20.0f;

        for (int i = 0; i < 4; ++i) {
            int xi = (i >> 0) & 1;
            int yi = (i >> 1) & 1;
            generate_chunk2(self, 2, xi, yi, 0);
            generate_chunk2(self, xi, 2, yi, 0);
            generate_chunk2(self, xi, yi, 2, 0);
        }
    }

    for (uint32_t chunk_index = 0; chunk_index < MAX_CHUNK_COUNT; ++chunk_index) {
        auto &chunk = self->chunks[chunk_index];
        if (!chunk) {
            continue;
        }

        auto brick_count = chunk->surface_brick_indices.size();

        if (chunk->bricks_changed) {
            int xi = chunk_index % CHUNK_NX;
            int yi = (chunk_index / CHUNK_NX) % CHUNK_NY;
            int zi = (chunk_index / CHUNK_NX / CHUNK_NY) % CHUNK_NZ;
            int li = (chunk_index / CHUNK_NX / CHUNK_NY / CHUNK_NZ);
            generate_chunk2(self, xi, yi, zi, li);
            brick_count = chunk->surface_brick_indices.size();
            if (chunk->render_chunk == nullptr) {
                chunk->render_chunk = create_chunk(g_renderer);
            }
            update(chunk->render_chunk, brick_count, chunk->surface_brick_indices.data(), chunk->voxel_brick_bitmasks.data(), reinterpret_cast<VoxelRenderAttribBrick const *const *>(chunk->voxel_brick_render_attribs.data()), (int const *)chunk->voxel_brick_positions.data());
            chunk->bricks_changed = false;
        }

        if (brick_count > 0) {
            render_chunk(g_renderer, chunk->render_chunk, (float const *)&chunk->pos);
        }
    }
}

#define HANDLE_RES(x, message) \
    if ((x) != GVOX_SUCCESS) { \
        return;                \
    }

void voxel_world::load_model(VoxelWorld self, char const *path) {
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

auto voxel_world::ray_cast(VoxelWorld self, RayCastConfig const &config) -> RayCastHit {
    auto [pos, face, dist] = dda_voxels(self, Ray{{config.ray_o[0], config.ray_o[1], config.ray_o[2]}, {config.ray_d[0], config.ray_d[1], config.ray_d[2]}}, config.max_iter, config.max_distance);
    return RayCastHit{.voxel_x = pos.x, .voxel_y = pos.y, .voxel_z = pos.z, .nrm_x = face.x, .nrm_y = face.y, .nrm_z = face.z, .distance = dist};
}

auto voxel_world::is_solid(VoxelWorld self, float const *pos) -> bool {
    auto p = glm::ivec3(glm::vec3(pos[0], pos[1], pos[2]) * 16.0f);
    return get_voxel_is_solid(self, p);
}

void fix_normals(voxel_world::VoxelWorld self, int const *pos) {
    for (int zi = -16; zi <= 16; ++zi) {
        for (int yi = -16; yi <= 16; ++yi) {
            for (int xi = -16; xi <= 16; ++xi) {
                auto p = glm::ivec3(pos[0], pos[1], pos[2]) + glm::ivec3(xi, yi, zi);

                float dx = get_voxel_sim_attrib(self, p + ivec3(1, 0, 0), true) - get_voxel_sim_attrib(self, p + ivec3(-1, 0, 0), true);
                float dy = get_voxel_sim_attrib(self, p + ivec3(0, 1, 0), true) - get_voxel_sim_attrib(self, p + ivec3(0, -1, 0), true);
                float dz = get_voxel_sim_attrib(self, p + ivec3(0, 0, 1), true) - get_voxel_sim_attrib(self, p + ivec3(0, 0, -1), true);

                auto prev_attrib = get_voxel_attrib(self, p);
                auto nrm = glm::normalize(glm::vec3(dx, dy, dz));

                // glm::vec2 uv0 = fract(glm::vec2(p.y, p.z) * 1.0f / 16.0f);
                // glm::vec2 uv1 = fract(glm::vec2(p.x, p.z) * 1.0f / 16.0f);
                // glm::vec2 uv2 = fract(glm::vec2(p.x, p.y) * 1.0f / 16.0f);

                // float uv0_a = abs(dot(nrm, vec3(1, 0, 0)));
                // float uv1_a = abs(dot(nrm, vec3(0, 1, 0)));
                // float uv2_a = abs(dot(nrm, vec3(0, 0, 1)));

                // auto col = glm::vec3(1.0, 0.1, 0.1) * uv0_a * uv0_a +
                //            glm::vec3(0.1, 1.0, 0.1) * uv1_a * uv1_a +
                //            glm::vec3(0.1, 0.1, 1.0) * uv2_a * uv2_a;

                set_voxel_attrib(self, p, Voxel{.col = prev_attrib.col, .nrm = {nrm.x, nrm.y, nrm.z}});
            }
        }
    }
}

void voxel_world::apply_brush_a(VoxelWorld self, int const *pos) {
    // place brush

    for (int zi = -15; zi <= 15; ++zi) {
        for (int yi = -15; yi <= 15; ++yi) {
            for (int xi = -15; xi <= 15; ++xi) {
                // float density = 15.0f - max(abs(xi), max(abs(yi), abs(zi)));
                float density = 15.0f - length(glm::vec3(xi, yi, zi));
                density = max(0.0f, density) / 20.0f;
                auto p = glm::ivec3(pos[0], pos[1], pos[2]) + glm::ivec3(xi, yi, zi);
                density += get_voxel_sim_attrib(self, p, true);
                set_voxel_bit(self, p, density < 0);
                set_voxel_sim_attrib(self, p, density);
            }
        }
    }

    fix_normals(self, pos);
}

void voxel_world::apply_brush_b(VoxelWorld self, int const *pos) {
    // break brush

    for (int zi = -15; zi <= 15; ++zi) {
        for (int yi = -15; yi <= 15; ++yi) {
            for (int xi = -15; xi <= 15; ++xi) {
                auto p = glm::ivec3(pos[0], pos[1], pos[2]) + glm::ivec3(xi, yi, zi);
                // float density = max(abs(xi), max(abs(yi), abs(zi))) - 15.0f;
                float density = length(glm::vec3(xi, yi, zi)) - 15.0f;
                density = min(0.0f, density) / 20.0f;
                if (density < 0) {
                    set_voxel_attrib(self, p, Voxel{.col = {1, 0.5f, 0}, .nrm = {}});
                }
                density += get_voxel_sim_attrib(self, p, true);
                set_voxel_bit(self, p, density < 0);
                set_voxel_sim_attrib(self, p, density);
            }
        }
    }

    fix_normals(self, pos);
}
