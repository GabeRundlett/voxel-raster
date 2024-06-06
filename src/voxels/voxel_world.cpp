#include "voxel_world.hpp"
#include "voxels/pack_unpack.inl"
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

constexpr int32_t CHUNK_NX = 8;
constexpr int32_t CHUNK_NY = 8;
constexpr int32_t CHUNK_NZ = 4;
constexpr int32_t CHUNK_LEVELS = 1;
constexpr mat3 m = mat3(0.00, 0.80, 0.60,
                        -0.80, 0.36, -0.48,
                        -0.60, -0.48, 0.64);
const mat3 mi = inverse(m);

auto get_brick_metadata(std::unique_ptr<Chunk> &chunk, auto brick_index) -> BrickMetadata & {
    return *reinterpret_cast<BrickMetadata *>(&chunk->voxel_brick_bitmasks[brick_index].metadata);
}

using Clock = std::chrono::steady_clock;

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
        auto minmax = voxel_minmax_value_cpp(RANDOM_VALUES.data(), p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);
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
                    auto p1 = p0 + VOXEL_BRICK_SIZE / 16.0f;
                    auto minmax = voxel_minmax_value_cpp(RANDOM_VALUES.data(), p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);
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
                generate_bitmask(brick_xi, brick_yi, brick_zi, chunk_xi, chunk_yi, chunk_zi, level, bitmask.bits, &bitmask.metadata, RANDOM_VALUES.data());
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
                    self->generate_chunk2s_total_n += 1;

                    generate_attributes(brick_xi, brick_yi, brick_zi, chunk_xi, chunk_yi, chunk_zi, level, (uint32_t *)attrib_brick->packed_voxels, RANDOM_VALUES.data());

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

    auto t1 = Clock::now();
    self->generate_chunk2s_total += (t1 - t0).count();

    if (update) {
        chunk->bricks_changed = true;
    }
}

void voxel_world::init(VoxelWorld &self) {
    self = new State{};
    self->prev_time = 0.0f;

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

    std::cout << "1: " << generate_chunk1s_main_total / 1'000'000 << " s | " << generate_chunk1s_main_total / self->generate_chunk1s_total_n << " us/brick (" << self->generate_chunk1s_total_n << " total bricks) " << generate_chunk1s_total / self->generate_chunk1s_total_n << " us/brick per thread" << std::endl;
    std::cout << "2: " << generate_chunk2s_main_total / 1'000'000 << " s | " << generate_chunk2s_main_total / self->generate_chunk2s_total_n << " us/brick (" << self->generate_chunk2s_total_n << " total bricks) " << generate_chunk2s_total / self->generate_chunk2s_total_n << " us/brick per thread" << std::endl;

    ISPCPrintInstrument();
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
