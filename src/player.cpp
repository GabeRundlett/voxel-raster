#include "player.hpp"

#include <glm/glm.hpp>

struct player::State {
    glm::vec3 pos;
    float yaw;
    float pitch;

    glm::vec3 forward;
    glm::vec2 forward_flat;
    glm::vec3 lateral;
    glm::vec2 lateral_flat;

    bool rot_dirty;
    bool trn_dirty;
    glm::mat4 view_to_world;
    glm::mat4 world_to_view;
};

void player::init(Player &self) {
    self = new State{};
    self->pos = glm::vec3{0};
    self->yaw = 0.0f;
    self->pitch = 0.0f;
    self->rot_dirty = true;
}
void player::deinit(Player self) {
    delete self;
}

void player::on_mouse_move(Player self, float x, float y) {
    float const SENS = 1.0f;
    float const MAX_ROT_EPS = 0.0f;
    float const M_PI = 3.14159265f;

    self->yaw += x * SENS * 0.001f;
    self->pitch -= y * SENS * 0.001f;
    self->pitch = glm::clamp(self->pitch, MAX_ROT_EPS, M_PI - MAX_ROT_EPS);

    self->rot_dirty = true;
}

void player::on_key(Player self, int key_id, int action) {
}

glm::mat4 rotation_matrix(float yaw, float pitch, float roll) {
    float sin_rot_x = sin(pitch), cos_rot_x = cos(pitch);
    float sin_rot_y = sin(roll), cos_rot_y = cos(roll);
    float sin_rot_z = sin(yaw), cos_rot_z = cos(yaw);
    return glm::mat4(
               cos_rot_z, -sin_rot_z, 0, 0,
               sin_rot_z, cos_rot_z, 0, 0,
               0, 0, 1, 0,
               0, 0, 0, 1) *
           glm::mat4(
               1, 0, 0, 0,
               0, cos_rot_x, sin_rot_x, 0,
               0, -sin_rot_x, cos_rot_x, 0,
               0, 0, 0, 1) *
           glm::mat4(
               cos_rot_y, -sin_rot_y, 0, 0,
               sin_rot_y, cos_rot_y, 0, 0,
               0, 0, 1, 0,
               0, 0, 0, 1);
}
glm::mat4 inv_rotation_matrix(float yaw, float pitch, float roll) {
    float sin_rot_x = sin(-pitch), cos_rot_x = cos(-pitch);
    float sin_rot_y = sin(-roll), cos_rot_y = cos(-roll);
    float sin_rot_z = sin(-yaw), cos_rot_z = cos(-yaw);
    return glm::mat4(
               cos_rot_y, -sin_rot_y, 0, 0,
               sin_rot_y, cos_rot_y, 0, 0,
               0, 0, 1, 0,
               0, 0, 0, 1) *
           glm::mat4(
               1, 0, 0, 0,
               0, cos_rot_x, sin_rot_x, 0,
               0, -sin_rot_x, cos_rot_x, 0,
               0, 0, 0, 1) *
           glm::mat4(
               cos_rot_z, -sin_rot_z, 0, 0,
               sin_rot_z, cos_rot_z, 0, 0,
               0, 0, 1, 0,
               0, 0, 0, 1);
}
glm::mat4 translation_matrix(glm::vec3 pos) {
    return glm::mat4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        pos.x, pos.y, pos.z, 1);
}

void player::update(Player self, float dt) {
    auto view_dirty = self->rot_dirty || self->trn_dirty;

    if (view_dirty) {
        self->view_to_world = translation_matrix(self->pos) * rotation_matrix(self->yaw, self->pitch, 0.0f);
        self->world_to_view = inv_rotation_matrix(self->yaw, self->pitch, 0.0f) * translation_matrix(self->pos * -1.0f);
    }

    if (self->rot_dirty) {
        float sin_rot_z = sinf(self->yaw), cos_rot_z = cosf(self->yaw);
        self->forward_flat = glm::vec3(+sin_rot_z, +cos_rot_z, 0);
        self->lateral_flat = glm::vec3(+cos_rot_z, -sin_rot_z, 0);
        self->forward = glm::vec3(self->view_to_world * glm::vec4(0, 0, 1, 0));
        self->lateral = glm::vec3(self->view_to_world * glm::vec4(1, 0, 0, 0));
    }

    self->rot_dirty = false;
    self->trn_dirty = false;
}
