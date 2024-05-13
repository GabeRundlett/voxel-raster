#include "voxel_world.hpp"
#include "voxels/voxel_mesh.inl"
#include <glm/glm.hpp>

struct voxel_world::State {
    std::vector<VoxelBrickBitmask> brick_bitmasks;
    std::vector<glm::ivec4> brick_positions;

    bool bricks_changed;
};

void voxel_world::init(VoxelWorld &self) {
    self = new State{};

    auto bitmask = VoxelBrickBitmask{};
    auto position = glm::ivec4{};

    for (uint32_t zi = 0; zi < VOXEL_BRICK_SIZE; ++zi) {
        for (uint32_t yi = 0; yi < VOXEL_BRICK_SIZE; ++yi) {
            for (uint32_t xi = 0; xi < VOXEL_BRICK_SIZE; ++xi) {
                uint32_t voxel_index = xi + yi * VOXEL_BRICK_SIZE + zi * VOXEL_BRICK_SIZE * VOXEL_BRICK_SIZE;
                uint32_t voxel_word_index = voxel_index / 32;
                uint32_t voxel_in_word_index = voxel_index % 32;
                uint32_t value = (xi + yi + zi) < 4 ? 1 : 0;
                bitmask.bits[voxel_word_index] |= uint32_t(value) << voxel_in_word_index;
            }
        }
    }

    for (int32_t zi = 0; zi < 16 / VOXEL_BRICK_SIZE; ++zi) {
        for (int32_t yi = 0; yi < 256 / VOXEL_BRICK_SIZE; ++yi) {
            for (int32_t xi = 0; xi < 256 / VOXEL_BRICK_SIZE; ++xi) {
                position = glm::ivec4{xi, yi, zi, 0};
                self->brick_bitmasks.push_back(bitmask);
                self->brick_positions.push_back(position);
            }
        }
    }

    self->bricks_changed = true;
}
void voxel_world::deinit(VoxelWorld self) {
    delete self;
}

VoxelBrickBitmask *voxel_world::get_voxel_brick_bitmasks(VoxelWorld self) {
    return self->brick_bitmasks.data();
}
int *voxel_world::get_voxel_brick_positions(VoxelWorld self) {
    return reinterpret_cast<int *>(self->brick_positions.data());
}
uint32_t voxel_world::get_voxel_brick_count(VoxelWorld self) {
    return self->brick_bitmasks.size();
}
bool voxel_world::bricks_changed(VoxelWorld self) {
    return self->bricks_changed;
}
void voxel_world::update(VoxelWorld self) {
    self->bricks_changed = false;
}
