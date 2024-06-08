#include "player.hpp"
#include "camera.inl"

#include <renderer/renderer.hpp>
#include <renderer/shared.inl>

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

struct CameraState {
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

void clear_move_state(CameraState *self) {
    self->move_f = false;
    self->move_b = false;
    self->move_l = false;
    self->move_r = false;
    self->move_u = false;
    self->move_d = false;
    self->move_flat = false;
    self->move_sprint = false;
    self->rot_dirty = true;
    self->trn_dirty = true;
    self->prj_dirty = true;
}

struct player::State {
    CameraState main_cam;
    CameraState prev_main_cam;
    CameraState observer_cam;
    CameraState prev_observer_cam;

    bool controlling_observer : 1;
    bool viewing_observer : 1;
};

float const SENS = 1.0f;
float const MAX_ROT_EPS = 0.0f;
float const M_PI = 3.14159265f;

void player::init(Player &self) {
    self = new State{};

    self->main_cam.pos = glm::vec3{0.0f, 0.0f, -1.0f};
    self->main_cam.yaw = 0.0f;
    self->main_cam.pitch = M_PI * 0.5f;

    self->main_cam.aspect = 1.0f;
    self->main_cam.vertical_fov_degrees = 74.0f;
    self->main_cam.near = 0.01f;
    self->main_cam.speed = 1.0f;

    self->main_cam.rot_dirty = true;
    self->main_cam.prj_dirty = true;

    self->observer_cam = self->main_cam;
    self->controlling_observer = false;
}
void player::deinit(Player self) {
    delete self;
}

void player::on_mouse_move(Player self, float x, float y) {
    auto on_mouse_move = [](CameraState *self, float x, float y) {
        self->yaw += x * SENS * 0.001f;
        self->pitch += y * SENS * 0.001f;
        self->pitch = glm::clamp(self->pitch, MAX_ROT_EPS, M_PI - MAX_ROT_EPS);

        self->rot_dirty = true;
    };
    if (self->controlling_observer) {
        on_mouse_move(&self->observer_cam, x, y);
    } else {
        on_mouse_move(&self->main_cam, x, y);
    }
}

void player::on_mouse_scroll(Player self, float x, float y) {
    auto on_mouse_scroll = [](CameraState *self, float x, float y) {
        if (y < 0) {
            self->speed *= 0.85f;
        } else {
            self->speed *= 1.2f;
        }
        self->speed = glm::clamp(self->speed, 1.0f, 64.0f);
    };
    if (self->controlling_observer) {
        on_mouse_scroll(&self->observer_cam, x, y);
    } else {
        on_mouse_scroll(&self->main_cam, x, y);
    }
}

void player::on_key(Player self, int key_id, int action) {
    auto on_key = [](CameraState *self, int key_id, int action) {
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
    };
    if (self->controlling_observer) {
        on_key(&self->observer_cam, key_id, action);
    } else {
        on_key(&self->main_cam, key_id, action);
    }

    if (key_id == GLFW_KEY_N && action == GLFW_PRESS && self->viewing_observer) {
        self->controlling_observer = !self->controlling_observer;
        if (!self->controlling_observer) {
            clear_move_state(&self->observer_cam);
        } else {
            clear_move_state(&self->main_cam);
        }
    }
    if (key_id == GLFW_KEY_P && action == GLFW_PRESS) {
        self->viewing_observer = !self->viewing_observer;
        self->controlling_observer = self->viewing_observer;
        if (!self->controlling_observer) {
            clear_move_state(&self->observer_cam);
        } else {
            clear_move_state(&self->main_cam);
        }
    }
    if (key_id == GLFW_KEY_O && action == GLFW_PRESS) {
        self->observer_cam = self->main_cam;
    }
}

void player::on_resize(Player self, int size_x, int size_y) {
    auto on_resize = [](CameraState *self, int size_x, int size_y) {
        self->aspect = float(size_x) / float(size_y);
        self->prj_dirty = true;
    };

    on_resize(&self->main_cam, size_x, size_y);
    on_resize(&self->observer_cam, size_x, size_y);
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

void update_camera(CameraState *self) {
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
    auto update = [](CameraState *self, CameraState *prev_self, float dt) {
        *prev_self = *self;
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
    };
    update(&self->observer_cam, &self->prev_observer_cam, dt);
    update(&self->main_cam, &self->prev_main_cam, dt);

    if (self->viewing_observer) {
        auto points = std::array<glm::vec3, 8>{
            glm::vec3{0, 0, 1},
            glm::vec3{1, 0, 1},
            glm::vec3{0, 1, 1},
            glm::vec3{1, 1, 1},
            glm::vec3{0, 0, 0.001f},
            glm::vec3{1, 0, 0.001f},
            glm::vec3{0, 1, 0.001f},
            glm::vec3{1, 1, 0.001f},
        };
        for (auto &point : points) {
            point.x = point.x * 2.0f - 1.0f;
            point.y = point.y * 2.0f - 1.0f;
            auto ws_p = self->main_cam.view_to_world * self->main_cam.clip_to_view * glm::vec4(point, 1.0f);
            point = glm::vec3(ws_p.x, ws_p.y, ws_p.z) / ws_p.w;
        }

        auto line_point_pairs = std::array{
            std::pair{0, 1},
            std::pair{1, 3},
            std::pair{3, 2},
            std::pair{2, 0},

            std::pair{0 + 4, 1 + 4},
            std::pair{1 + 4, 3 + 4},
            std::pair{3 + 4, 2 + 4},
            std::pair{2 + 4, 0 + 4},

            std::pair{0, 0 + 4},
            std::pair{1, 1 + 4},
            std::pair{2, 2 + 4},
            std::pair{3, 3 + 4},
        };

        for (auto const &[pi0, pi1] : line_point_pairs) {
            using Line = std::array<glm::vec3, 2>;
            auto line = Line{points[pi0], points[pi1]};
            renderer::submit_debug_lines((float const *)&line, 1);
        }
    }
}

void get_camera(CameraState const &cam, CameraState const &prev_cam, Camera *camera, GpuInput const *gpu_input) {
    camera->world_to_view = std::bit_cast<daxa_f32mat4x4>(cam.world_to_view);
    camera->view_to_world = std::bit_cast<daxa_f32mat4x4>(cam.view_to_world);
    camera->view_to_clip = std::bit_cast<daxa_f32mat4x4>(cam.view_to_clip);
    camera->clip_to_view = std::bit_cast<daxa_f32mat4x4>(cam.clip_to_view);

    camera->prev_world_to_prev_view = std::bit_cast<daxa_f32mat4x4>(prev_cam.world_to_view);
    camera->prev_view_to_prev_world = std::bit_cast<daxa_f32mat4x4>(prev_cam.view_to_world);
    camera->prev_view_to_prev_clip = std::bit_cast<daxa_f32mat4x4>(prev_cam.view_to_clip);
    camera->prev_clip_to_prev_view = std::bit_cast<daxa_f32mat4x4>(prev_cam.clip_to_view);

    daxa_f32vec2 sample_offset = daxa_f32vec2(
        gpu_input->jitter.x / float(gpu_input->render_size.x),
        gpu_input->jitter.y / float(gpu_input->render_size.y));

    glm::mat4 clip_to_sample = glm::mat4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        sample_offset.x * -1.0f, sample_offset.y * -1.0f, 0, 1);

    glm::mat4 sample_to_clip = glm::mat4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        sample_offset.x * +1.0f, sample_offset.y * +1.0f, 0, 1);

    camera->view_to_sample = std::bit_cast<daxa_f32mat4x4>(clip_to_sample * cam.view_to_clip);
    camera->sample_to_view = std::bit_cast<daxa_f32mat4x4>(cam.clip_to_view * sample_to_clip);

    camera->clip_to_prev_clip = std::bit_cast<daxa_f32mat4x4>(
        prev_cam.view_to_clip *
        prev_cam.world_to_view *
        cam.view_to_world *
        cam.clip_to_view);
}

void player::get_camera(Player self, Camera *camera, GpuInput const *gpu_input) {
    update_camera(&self->main_cam);
    get_camera(self->main_cam, self->prev_main_cam, camera, gpu_input);
}

void player::get_observer_camera(Player self, Camera *camera, GpuInput const *gpu_input) {
    update_camera(&self->observer_cam);
    get_camera(self->observer_cam, self->prev_observer_cam, camera, gpu_input);
}

auto player::should_draw_from_observer(Player self) -> bool {
    return self->viewing_observer;
}
