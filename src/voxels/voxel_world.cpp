#include "voxel_world.hpp"
#include "voxels/voxel_mesh.inl"

#include <glm/common.hpp>
#include <glm/glm.hpp>

#include <gvox/gvox.h>
#include <gvox/streams/input/byte_buffer.h>
#include <gvox/containers/raw.h>

#include <fstream>
#include <array>

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
    std::vector<VoxelBrickBitmask> surface_brick_bitmasks;
    std::vector<glm::ivec4> surface_brick_positions;
    glm::vec3 pos;
    bool bricks_changed;
};

struct voxel_world::State {
    std::array<std::unique_ptr<Chunk>, MAX_CHUNK_COUNT> chunks;

    // std::vector<std::byte> brick_aux_buffer;
    // std::vector<GvoxBrick> brick_heap;
    // std::vector<uint64_t> brick_free_list;
};

void voxel_world::init(VoxelWorld &self) {
    self = new State{};

    for (uint32_t chunk_index = 0; chunk_index < 1; ++chunk_index) {
        auto &chunk = self->chunks[chunk_index];
        chunk = std::make_unique<Chunk>();
        chunk->pos = {chunk_index * 1.0f, 0.0f, 0.0f};

        auto get_brick_metadata = [&](auto brick_index) -> BrickMetadata & {
            return *reinterpret_cast<BrickMetadata*>(&chunk->voxel_brick_bitmasks[brick_index].metadata);
        };

        for (int32_t brick_zi = 0; brick_zi < BRICK_CHUNK_SIZE; ++brick_zi) {
            for (int32_t brick_yi = 0; brick_yi < BRICK_CHUNK_SIZE; ++brick_yi) {
                for (int32_t brick_xi = 0; brick_xi < BRICK_CHUNK_SIZE; ++brick_xi) {
                    auto brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                    auto &brick_metadata = get_brick_metadata(brick_index);
                    auto &bitmask = chunk->voxel_brick_bitmasks[brick_index];

                    brick_metadata = {};
                    bitmask = {};

                    for (uint32_t zi = 0; zi < VOXEL_BRICK_SIZE; ++zi) {
                        for (uint32_t yi = 0; yi < VOXEL_BRICK_SIZE; ++yi) {
                            for (uint32_t xi = 0; xi < VOXEL_BRICK_SIZE; ++xi) {
                                uint32_t voxel_index = xi + yi * VOXEL_BRICK_SIZE + zi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
                                uint32_t voxel_word_index = voxel_index / 32;
                                uint32_t voxel_in_word_index = voxel_index % 32;
                                float x = ((float(xi + brick_xi * VOXEL_BRICK_SIZE) + 0.5f) - 0.5f * VOXEL_CHUNK_SIZE) / 16.0f;
                                float y = ((float(yi + brick_yi * VOXEL_BRICK_SIZE) + 0.5f) - 0.5f * VOXEL_CHUNK_SIZE) / 16.0f;
                                float z = ((float(zi + brick_zi * VOXEL_BRICK_SIZE) + 0.5f) - 0.5f * VOXEL_CHUNK_SIZE) / 16.0f;

                                x = glm::fract(x * 0.25f) * 4.0f - 2.0f;
                                y = glm::fract(y * 0.25f) * 4.0f - 2.0f;
                                z = glm::fract(z * 0.25f) * 4.0f - 2.0f;

                                uint32_t value = (x * x + y * y + z * z) < float(2 * 2) ? 1 : 0;
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
                                bitmask.bits[voxel_word_index] |= uint32_t(value) << voxel_in_word_index;
                            }
                        }
                    }
                }
            }
        }

        for (int32_t brick_zi = 0; brick_zi < BRICK_CHUNK_SIZE; ++brick_zi) {
            for (int32_t brick_yi = 0; brick_yi < BRICK_CHUNK_SIZE; ++brick_yi) {
                for (int32_t brick_xi = 0; brick_xi < BRICK_CHUNK_SIZE; ++brick_xi) {
                    auto brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                    auto &brick_metadata = get_brick_metadata(brick_index);
                    auto &bitmask = chunk->voxel_brick_bitmasks[brick_index];

                    brick_metadata.exposed_nx = true;
                    brick_metadata.exposed_px = true;
                    brick_metadata.exposed_ny = true;
                    brick_metadata.exposed_py = true;
                    brick_metadata.exposed_nz = true;
                    brick_metadata.exposed_pz = true;

                    if (brick_xi != 0) {
                        auto neighbor_brick_index = (brick_xi - 1) + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_brick_index);
                        brick_metadata.exposed_nx = neighbor_brick_metadata.has_air_px;
                    }
                    if (brick_yi != 0) {
                        auto neighbor_brick_index = brick_xi + (brick_yi - 1) * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_brick_index);
                        brick_metadata.exposed_ny = neighbor_brick_metadata.has_air_py;
                    }
                    if (brick_zi != 0) {
                        auto neighbor_brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + (brick_zi - 1) * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_brick_index);
                        brick_metadata.exposed_nz = neighbor_brick_metadata.has_air_pz;
                    }
                    if (brick_xi != BRICK_CHUNK_SIZE - 1) {
                        auto neighbor_brick_index = (brick_xi + 1) + brick_yi * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_brick_index);
                        brick_metadata.exposed_px = neighbor_brick_metadata.has_air_nx;
                    }
                    if (brick_yi != BRICK_CHUNK_SIZE - 1) {
                        auto neighbor_brick_index = brick_xi + (brick_yi + 1) * BRICK_CHUNK_SIZE + brick_zi * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_brick_index);
                        brick_metadata.exposed_py = neighbor_brick_metadata.has_air_ny;
                    }
                    if (brick_zi != BRICK_CHUNK_SIZE - 1) {
                        auto neighbor_brick_index = brick_xi + brick_yi * BRICK_CHUNK_SIZE + (brick_zi + 1) * BRICK_CHUNK_SIZE * BRICK_CHUNK_SIZE;
                        auto &neighbor_brick_metadata = get_brick_metadata(neighbor_brick_index);
                        brick_metadata.exposed_pz = neighbor_brick_metadata.has_air_nz;
                    }

                    bool exposed = brick_metadata.exposed_nx || brick_metadata.exposed_px || brick_metadata.exposed_ny || brick_metadata.exposed_py || brick_metadata.exposed_nz || brick_metadata.exposed_pz;

                    auto position = glm::ivec4{brick_xi, brick_yi, brick_zi, 0};
                    if (brick_metadata.has_voxel && exposed) {
                        chunk->surface_brick_bitmasks.push_back(bitmask);
                        chunk->surface_brick_positions.push_back(position);
                    }
                }
            }
        }

        chunk->bricks_changed = true;
    }
}
void voxel_world::deinit(VoxelWorld self) {
    delete self;
}

unsigned int voxel_world::get_chunk_count(VoxelWorld self) {
    return 1;
}

VoxelBrickBitmask *voxel_world::get_voxel_brick_bitmasks(VoxelWorld self, unsigned int chunk_index) {
    return self->chunks[chunk_index]->surface_brick_bitmasks.data();
}
int *voxel_world::get_voxel_brick_positions(VoxelWorld self, unsigned int chunk_index) {
    return reinterpret_cast<int *>(self->chunks[chunk_index]->surface_brick_positions.data());
}
uint32_t voxel_world::get_voxel_brick_count(VoxelWorld self, unsigned int chunk_index) {
    return self->chunks[chunk_index]->surface_brick_bitmasks.size();
}
bool voxel_world::chunk_bricks_changed(VoxelWorld self, unsigned int chunk_index) {
    return self->chunks[chunk_index]->bricks_changed;
}
void voxel_world::update(VoxelWorld self) {
    for (uint32_t chunk_index = 0; chunk_index < MAX_CHUNK_COUNT; ++chunk_index) {
        auto &chunk = self->chunks[chunk_index];
        if (!chunk) {
            continue;
        }
        chunk->bricks_changed = false;
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
