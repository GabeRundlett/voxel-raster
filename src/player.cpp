#include "player.hpp"
#include "camera.inl"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

struct player::State {
    glm::vec3 pos;
    float yaw;
    float pitch;

    glm::vec3 forward;
    glm::vec2 forward_flat;
    glm::vec3 lateral;
    glm::vec2 lateral_flat;
    glm::vec3 upwards;

    float aspect;
    float vertical_fov_degrees;
    float near;
    float speed;

    glm::mat4 view_to_world;
    glm::mat4 world_to_view;
    glm::mat4 view_to_clip;
    glm::mat4 clip_to_view;

    bool move_f : 1;
    bool move_b : 1;
    bool move_l : 1;
    bool move_r : 1;
    bool move_u : 1;
    bool move_d : 1;
    bool move_flat : 1;
    bool move_sprint : 1;
    bool rot_dirty : 1;
    bool trn_dirty : 1;
    bool prj_dirty : 1;
};

float const SENS = 1.0f;
float const MAX_ROT_EPS = 0.0f;
float const M_PI = 3.14159265f;

void player::init(Player &self) {
    self = new State{};

    self->pos = glm::vec3{0.0f, 0.0f, -1.0f};
    self->yaw = 0.0f;
    self->pitch = M_PI * 0.5f;

    self->aspect = 1.0f;
    self->vertical_fov_degrees = 74.0f;
    self->near = 0.01f;
    self->speed = 1.0f;

    self->rot_dirty = true;
    self->prj_dirty = true;
}
void player::deinit(Player self) {
    delete self;
}

void player::on_mouse_move(Player self, float x, float y) {
    self->yaw += x * SENS * 0.001f;
    self->pitch += y * SENS * 0.001f;
    self->pitch = glm::clamp(self->pitch, MAX_ROT_EPS, M_PI - MAX_ROT_EPS);

    self->rot_dirty = true;
}

void player::on_mouse_scroll(Player self, float x, float y) {
    if (y < 0) {
        self->speed *= 0.85f;
    } else {
        self->speed *= 1.2f;
    }
    self->speed = glm::clamp(self->speed, 1.0f, 64.0f);
}

void player::on_key(Player self, int key_id, int action) {
    if (key_id == GLFW_KEY_W) {
        self->move_f = action != GLFW_RELEASE;
    }
    if (key_id == GLFW_KEY_S) {
        self->move_b = action != GLFW_RELEASE;
    }
    if (key_id == GLFW_KEY_A) {
        self->move_l = action != GLFW_RELEASE;
    }
    if (key_id == GLFW_KEY_D) {
        self->move_r = action != GLFW_RELEASE;
    }
    if (key_id == GLFW_KEY_SPACE) {
        self->move_u = action != GLFW_RELEASE;
    }
    if (key_id == GLFW_KEY_LEFT_CONTROL) {
        self->move_d = action != GLFW_RELEASE;
    }
    if (key_id == GLFW_KEY_Q && action == GLFW_RELEASE) {
        self->move_flat = !self->move_flat;
    }
    if (key_id == GLFW_KEY_LEFT_SHIFT) {
        self->move_sprint = action != GLFW_RELEASE;
    }
}

void player::on_resize(Player self, int size_x, int size_y) {
    self->aspect = float(size_x) / float(size_y);
    self->prj_dirty = true;
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

void update_camera(player::Player self) {
    auto view_dirty = self->rot_dirty || self->trn_dirty;

    if (self->rot_dirty) {
        float sin_rot_z = sinf(self->yaw), cos_rot_z = cosf(self->yaw);
        self->forward_flat = glm::vec3(+sin_rot_z, +cos_rot_z, 0);
        self->lateral_flat = glm::vec3(+cos_rot_z, -sin_rot_z, 0);
    }

    if (view_dirty) {
        self->view_to_world = translation_matrix(self->pos) * rotation_matrix(self->yaw, self->pitch, 0.0f);
        self->world_to_view = inv_rotation_matrix(self->yaw, self->pitch, 0.0f) * translation_matrix(self->pos * -1.0f);
    }

    if (self->rot_dirty) {
        self->forward = glm::vec3(self->view_to_world * glm::vec4(0, 0, -1, 0));
        self->lateral = glm::vec3(self->view_to_world * glm::vec4(+1, 0, 0, 0));
        self->upwards = glm::vec3(self->view_to_world * glm::vec4(0, -1, 0, 0));
    }

    if (self->prj_dirty) {
        auto fov = glm::radians(self->vertical_fov_degrees);
        auto tan_half_fov = tan(fov * 0.5f);
        auto aspect = self->aspect;
        auto near = self->near;

        self->view_to_clip = glm::mat4{0.0f};
        self->view_to_clip[0][0] = +1.0f / tan_half_fov / aspect;
        self->view_to_clip[1][1] = +1.0f / tan_half_fov;
        self->view_to_clip[2][2] = +0.0f;
        self->view_to_clip[2][3] = -1.0f;
        self->view_to_clip[3][2] = near;

        self->clip_to_view = glm::mat4{0.0f};
        self->clip_to_view[0][0] = tan_half_fov * aspect;
        self->clip_to_view[1][1] = tan_half_fov;
        self->clip_to_view[2][2] = +0.0f;
        self->clip_to_view[2][3] = +1.0f / near;
        self->clip_to_view[3][2] = -1.0f;
    }

    self->rot_dirty = false;
    self->trn_dirty = false;
    self->prj_dirty = false;
}

void player::update(Player self, float dt) {
    float const speed = (self->move_sprint ? 10.0f : 1.0f) * self->speed;
    if (self->move_flat) {
        if (self->move_f) {
            self->pos += glm::vec3(self->forward_flat, 0) * (speed * dt);
            self->trn_dirty = true;
        }
        if (self->move_b) {
            self->pos -= glm::vec3(self->forward_flat, 0) * (speed * dt);
            self->trn_dirty = true;
        }
        if (self->move_u) {
            self->pos += glm::vec3(0, 0, -1) * (speed * dt);
            self->trn_dirty = true;
        }
        if (self->move_d) {
            self->pos -= glm::vec3(0, 0, -1) * (speed * dt);
            self->trn_dirty = true;
        }
    } else {
        if (self->move_f) {
            self->pos += self->forward * (speed * dt);
            self->trn_dirty = true;
        }
        if (self->move_b) {
            self->pos -= self->forward * (speed * dt);
            self->trn_dirty = true;
        }
        if (self->move_u) {
            self->pos += self->upwards * (speed * dt);
            self->trn_dirty = true;
        }
        if (self->move_d) {
            self->pos -= self->upwards * (speed * dt);
            self->trn_dirty = true;
        }
    }
    if (self->move_l) {
        self->pos -= self->lateral * (speed * dt);
        self->trn_dirty = true;
    }
    if (self->move_r) {
        self->pos += self->lateral * (speed * dt);
        self->trn_dirty = true;
    }
    update_camera(self);
}

void player::get_camera(Player self, Camera *camera) {
    update_camera(self);
    camera->world_to_view = std::bit_cast<daxa_f32mat4x4>(self->world_to_view);
    camera->view_to_world = std::bit_cast<daxa_f32mat4x4>(self->view_to_world);
    camera->view_to_clip = std::bit_cast<daxa_f32mat4x4>(self->view_to_clip);
    camera->clip_to_view = std::bit_cast<daxa_f32mat4x4>(self->clip_to_view);
}
