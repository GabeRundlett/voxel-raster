#include "player.hpp"
#include "camera.inl"

#include <renderer/renderer.hpp>
#include <voxels/voxel_world.hpp>
#include <renderer/shared.inl>

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

struct CameraState {
    glm::mat4 view_to_world;
    glm::mat4 world_to_view;
    glm::mat4 view_to_clip;
    glm::mat4 clip_to_view;
};

struct Controller {
    glm::vec3 pos;
    glm::vec3 cam_pos_offset;
    glm::vec3 vel;
    float yaw;
    float pitch;
    float speed;

    float aspect;
    float vertical_fov_degrees;
    float near;

    glm::vec3 forward;
    glm::vec2 forward_flat;
    glm::vec3 lateral;
    glm::vec2 lateral_flat;
    glm::vec3 upwards;

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

    bool is_flying : 1;
    bool on_ground : 1;
    bool is_crouched : 1;

    CameraState prev_cam;
    CameraState cam;
};

void clear_move_state(Controller *self) {
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
    Controller main;
    Controller observer;

    bool controlling_observer : 1;
    bool viewing_observer : 1;
};

float const SENS = 1.0f;
float const MAX_ROT_EPS = 0.0f;
float const M_PI = 3.14159265f;

void player::init(Player &self) {
    self = new State{};

    self->main.pos = glm::vec3{40.0f, 40.0f, 40.0f};
    self->main.yaw = 0.0f;
    self->main.pitch = M_PI * 0.5f;
    self->main.speed = 1.5f;

    self->main.rot_dirty = true;
    self->main.prj_dirty = true;
    self->main.is_flying = true;
    self->main.on_ground = false;

    self->main.aspect = 1.0f;
    self->main.vertical_fov_degrees = 74.0f;
    self->main.near = 0.01f;

    self->observer = self->main;
    self->controlling_observer = false;
}
void player::deinit(Player self) {
    delete self;
}

void player::on_mouse_move(Player self, float x, float y) {
    auto on_mouse_move = [](Controller *self, float x, float y) {
        self->yaw += x * SENS * 0.001f;
        self->pitch -= y * SENS * 0.001f;
        self->pitch = glm::clamp(self->pitch, MAX_ROT_EPS, M_PI - MAX_ROT_EPS);

        self->rot_dirty = true;
    };
    if (self->controlling_observer) {
        on_mouse_move(&self->observer, x, y);
    } else {
        on_mouse_move(&self->main, x, y);
    }
}

void player::on_mouse_scroll(Player self, float x, float y) {
    auto on_mouse_scroll = [](Controller *self, float x, float y) {
        if (y < 0) {
            self->speed *= 0.85f;
        } else {
            self->speed *= 1.2f;
        }
        self->speed = glm::clamp(self->speed, 1.5f, 64.0f);
    };
    if (self->controlling_observer) {
        on_mouse_scroll(&self->observer, x, y);
    } else {
        on_mouse_scroll(&self->main, x, y);
    }
}

void player::on_key(Player self, int key_id, int action) {
    auto on_key = [](Controller *self, int key_id, int action) {
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
        if (key_id == GLFW_KEY_F && action == GLFW_PRESS) {
            self->is_flying = !self->is_flying;
        }
    };
    if (self->controlling_observer) {
        on_key(&self->observer, key_id, action);
    } else {
        on_key(&self->main, key_id, action);
    }

    if (key_id == GLFW_KEY_N && action == GLFW_PRESS && self->viewing_observer) {
        self->controlling_observer = !self->controlling_observer;
        if (!self->controlling_observer) {
            clear_move_state(&self->observer);
        } else {
            clear_move_state(&self->main);
        }
    }
    if (key_id == GLFW_KEY_P && action == GLFW_PRESS) {
        self->viewing_observer = !self->viewing_observer;
        self->controlling_observer = self->viewing_observer;
        if (!self->controlling_observer) {
            clear_move_state(&self->observer);
        } else {
            clear_move_state(&self->main);
        }
    }
    if (key_id == GLFW_KEY_O && action == GLFW_PRESS) {
        self->observer = self->main;
    }
}

void player::on_resize(Player self, int size_x, int size_y) {
    auto on_resize = [](Controller *self, int size_x, int size_y) {
        self->aspect = float(size_x) / float(size_y);
        self->prj_dirty = true;
    };

    on_resize(&self->main, size_x, size_y);
    on_resize(&self->observer, size_x, size_y);
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

float const standing_height = 1.75f;
float const crouch_height = 1.0f;
float const jump_strength = 1.0f;

float const VOXEL_SCL = 16.0f;
float const VOXEL_SIZE = 1.0f / VOXEL_SCL;

glm::vec3 const eye_offset = glm::vec3(0, 0, -0.2f);

void update_camera(Controller *self) {
    auto view_dirty = self->rot_dirty || self->trn_dirty || true;

    if (self->rot_dirty) {
        float sin_rot_z = sinf(self->yaw), cos_rot_z = cosf(self->yaw);
        self->forward_flat = glm::vec3(+sin_rot_z, +cos_rot_z, 0);
        self->lateral_flat = glm::vec3(+cos_rot_z, -sin_rot_z, 0);
    }

    if (view_dirty) {
        self->cam.view_to_world = translation_matrix(self->pos + self->cam_pos_offset + eye_offset) * rotation_matrix(self->yaw, self->pitch, 0.0f);
        self->cam.world_to_view = inv_rotation_matrix(self->yaw, self->pitch, 0.0f) * translation_matrix((self->pos + self->cam_pos_offset + eye_offset) * -1.0f);
    }

    if (self->rot_dirty) {
        self->forward = glm::vec3(self->cam.view_to_world * glm::vec4(0, 0, -1, 0));
        self->lateral = glm::vec3(self->cam.view_to_world * glm::vec4(+1, 0, 0, 0));
        self->upwards = glm::vec3(self->cam.view_to_world * glm::vec4(0, +1, 0, 0));
    }

    if (self->prj_dirty) {
        auto fov = glm::radians(self->vertical_fov_degrees);
        auto tan_half_fov = tan(fov * 0.5f);
        auto aspect = self->aspect;
        auto near = self->near;

        self->cam.view_to_clip = glm::mat4{0.0f};
        self->cam.view_to_clip[0][0] = +1.0f / tan_half_fov / aspect;
        self->cam.view_to_clip[1][1] = -1.0f / tan_half_fov;
        self->cam.view_to_clip[2][2] = +0.0f;
        self->cam.view_to_clip[2][3] = -1.0f;
        self->cam.view_to_clip[3][2] = near;

        self->cam.clip_to_view = glm::mat4{0.0f};
        self->cam.clip_to_view[0][0] = tan_half_fov * aspect;
        self->cam.clip_to_view[1][1] = -tan_half_fov;
        self->cam.clip_to_view[2][2] = +0.0f;
        self->cam.clip_to_view[2][3] = +1.0f / near;
        self->cam.clip_to_view[3][2] = -1.0f;
    }

    self->rot_dirty = false;
    self->trn_dirty = false;
    self->prj_dirty = false;
}

#define EARTH_GRAV 9.807f
#define MOON_GRAV 1.625f
#define MARS_GRAV 3.728f
#define JUPITER_GRAV 25.93f

#define GRAVITY EARTH_GRAV

void player::update(Player self, float dt) {
    auto update = [](Controller *self, float dt) {
        self->prev_cam = self->cam;
        float const speed = (self->move_sprint ? (self->is_flying ? 10.0f : 3.0f) : 1.0f) * self->speed;
        float height = standing_height;

        auto move_vec = glm::vec3(0);
        auto acc = glm::vec3(0);

        if (self->is_flying) {
            self->vel = glm::vec3(0);
            if (self->move_flat) {
                if (self->move_f) {
                    move_vec += glm::vec3(self->forward_flat, 0);
                }
                if (self->move_b) {
                    move_vec -= glm::vec3(self->forward_flat, 0);
                }
                if (self->move_u) {
                    move_vec += glm::vec3(0, 0, 1);
                }
                if (self->move_d) {
                    move_vec -= glm::vec3(0, 0, 1);
                }
            } else {
                if (self->move_f) {
                    move_vec += self->forward;
                }
                if (self->move_b) {
                    move_vec -= self->forward;
                }
                if (self->move_u) {
                    move_vec += self->upwards;
                }
                if (self->move_d) {
                    move_vec -= self->upwards;
                }
            }
            if (self->move_l) {
                move_vec -= self->lateral;
            }
            if (self->move_r) {
                move_vec += self->lateral;
            }
        } else {
            if (self->move_f) {
                move_vec += glm::vec3(self->forward_flat, 0);
            }
            if (self->move_b) {
                move_vec -= glm::vec3(self->forward_flat, 0);
            }
            if (self->move_l) {
                move_vec -= self->lateral;
            }
            if (self->move_r) {
                move_vec += self->lateral;
            }

            if (self->on_ground && self->move_u) {
                self->vel.z = EARTH_GRAV * sqrt(jump_strength * 2.0 / EARTH_GRAV);
            } else {
                acc.z -= GRAVITY;
            }

            if (self->move_d != 0) {
                if (!self->is_crouched) {
                    self->pos.z -= height - crouch_height;
                    self->cam_pos_offset.z += height - crouch_height;
                }
                self->is_crouched = true;
                height = crouch_height;
            } else {
                if (self->is_crouched) {
                    self->pos.z += height - crouch_height;
                    self->cam_pos_offset.z -= height - crouch_height;
                }
                self->is_crouched = false;
            }
        }

        self->vel += acc * dt;
        auto vel = self->vel + move_vec * speed;
        if (length(vel) > 0.0f) {
            self->trn_dirty = true;
        }
        self->pos += vel * dt;

        self->on_ground = false;
        bool inside_terrain = false;
        int32_t voxel_height = height * VOXEL_SCL + 1;

        for (int32_t xi = -2; xi <= 2; ++xi) {
            for (int32_t yi = -2; yi <= 2; ++yi) {
                for (int32_t zi = 0; zi <= voxel_height; ++zi) {
                    auto p = self->pos - glm::vec3(0, 0, height) + glm::vec3(xi * VOXEL_SIZE, yi * VOXEL_SIZE, zi * VOXEL_SIZE);
                    auto in_voxel = voxel_world::is_solid(&p.x);
                    if (in_voxel) {
                        inside_terrain = true;
                        break;
                    }
                }
            }
        }

        if (inside_terrain) {
            bool space_above = false;
            int32_t first_height = -1;
            for (int32_t zi = 0; zi < voxel_height + voxel_height / 2; ++zi) {
                bool found_voxel = false;
                for (int32_t xi = -2; xi <= 2; ++xi) {
                    if (found_voxel) {
                        break;
                    }
                    for (int32_t yi = -2; yi <= 2; ++yi) {
                        if (found_voxel) {
                            break;
                        }
                        auto p = self->pos - glm::vec3(0, 0, height) + glm::vec3(xi * VOXEL_SIZE, yi * VOXEL_SIZE, zi * VOXEL_SIZE);
                        auto solid = voxel_world::is_solid(&p.x);
                        if (solid) {
                            found_voxel = true;
                        }
                    }
                }
                if (zi - first_height >= voxel_height) {
                    break;
                }
                if (found_voxel) {
                    first_height = -1;
                    space_above = false;
                }
                if (!found_voxel && zi < voxel_height / 2 && first_height == -1) {
                    first_height = zi;
                    space_above = true;
                }
            }
            if (space_above) {
                float current_z = self->pos.z;
                self->pos = self->pos + glm::vec3(0, 0, VOXEL_SIZE * first_height);
                self->pos.z = floor(self->pos.z * VOXEL_SCL) * VOXEL_SIZE;
                float new_z = self->pos.z;
                self->cam_pos_offset.z += current_z - new_z;
                self->on_ground = true;
                self->vel.z = 0;
            } else {
                self->pos -= vel * dt;
            }
        }

        auto cam_pos_offset_sign = sign(self->cam_pos_offset);
        auto const interp_speed = std::max(length(self->cam_pos_offset) * float(VOXEL_SCL), 0.25f);
        self->cam_pos_offset = self->cam_pos_offset - cam_pos_offset_sign * dt * interp_speed;

        auto new_cam_pos_offset_sign = sign(self->cam_pos_offset);
        if (new_cam_pos_offset_sign.x != cam_pos_offset_sign.x)
            self->cam_pos_offset.x = 0.0f;
        if (new_cam_pos_offset_sign.y != cam_pos_offset_sign.y)
            self->cam_pos_offset.y = 0.0f;
        if (new_cam_pos_offset_sign.z != cam_pos_offset_sign.z)
            self->cam_pos_offset.z = 0.0f;

        update_camera(self);
    };
    update(&self->observer, dt);
    update(&self->main, dt);
    using Box = std::array<glm::vec3, 3>;

    if (self->viewing_observer) {
        using Line = std::array<glm::vec3, 3>;
        using Point = std::array<glm::vec3, 3>;

        // Draw main camera frustum outline:
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
            auto ws_p = self->main.cam.view_to_world * self->main.cam.clip_to_view * glm::vec4(point, 1.0f);
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
            auto line = Line{points[pi0], points[pi1], {1.0f, 1.0f, 1.0f}};
            renderer::submit_debug_lines((renderer::Line const *)&line, 1);

            // auto pt = Point{points[pi0], {0.0f, 1.0f, 1.0f}, {0.125f, 0.125f, 1.0f}};
            // renderer::submit_debug_points((renderer::Point const *)&pt, 1);
        }

        {
            float const height = self->main.is_crouched ? crouch_height : standing_height;
            auto p = self->main.pos - glm::vec3(0, 0, height) + glm::vec3(-2 * VOXEL_SIZE, -2 * VOXEL_SIZE, -2 * VOXEL_SIZE);
            int32_t voxel_height = height * VOXEL_SCL + 1;
            auto cube = Box{p, p + glm::vec3(5, 5, voxel_height) * VOXEL_SIZE, {1.0f, 1.0f, 1.0f}};
            renderer::submit_debug_box_lines((renderer::Box const *)&cube, 1);
        }
    }

    auto ray_pos = self->main.pos + self->main.cam_pos_offset + eye_offset;
    auto ray_cast = voxel_world::ray_cast(&ray_pos.x, &self->main.forward.x);
    if (ray_cast.distance != -1.0f && ray_cast.distance / 16.0f < 10.0f) {
        auto cube = Box{
            glm::vec3(ray_cast.voxel_x, ray_cast.voxel_y, ray_cast.voxel_z) / 16.0f,
            glm::vec3(ray_cast.voxel_x + 1, ray_cast.voxel_y + 1, ray_cast.voxel_z + 1) / 16.0f,
            {1.0f, 1.0f, 1.0f},
        };
        renderer::submit_debug_box_lines((renderer::Box const *)&cube, 1);
    }
}

void get_camera(CameraState const &cam, CameraState const &prev_cam, Camera *camera, GpuInput const *gpu_input, bool should_jitter) {
    camera->world_to_view = std::bit_cast<daxa_f32mat4x4>(cam.world_to_view);
    camera->view_to_world = std::bit_cast<daxa_f32mat4x4>(cam.view_to_world);
    camera->view_to_clip = std::bit_cast<daxa_f32mat4x4>(cam.view_to_clip);
    camera->clip_to_view = std::bit_cast<daxa_f32mat4x4>(cam.clip_to_view);

    camera->prev_world_to_prev_view = std::bit_cast<daxa_f32mat4x4>(prev_cam.world_to_view);
    camera->prev_view_to_prev_world = std::bit_cast<daxa_f32mat4x4>(prev_cam.view_to_world);
    camera->prev_view_to_prev_clip = std::bit_cast<daxa_f32mat4x4>(prev_cam.view_to_clip);
    camera->prev_clip_to_prev_view = std::bit_cast<daxa_f32mat4x4>(prev_cam.clip_to_view);

    if (should_jitter) {
        daxa_f32vec2 sample_offset = daxa_f32vec2(
            gpu_input->jitter.x / float(gpu_input->render_size.x),
            gpu_input->jitter.y / float(gpu_input->render_size.y));

        glm::mat4 clip_to_sample = glm::mat4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            sample_offset.x * +2.0f, sample_offset.y * +2.0f, 0, 1);

        glm::mat4 sample_to_clip = glm::mat4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            sample_offset.x * -2.0f, sample_offset.y * -2.0f, 0, 1);

        camera->view_to_sample = std::bit_cast<daxa_f32mat4x4>(clip_to_sample * cam.view_to_clip);
        camera->sample_to_view = std::bit_cast<daxa_f32mat4x4>(cam.clip_to_view * sample_to_clip);
    } else {
        camera->view_to_sample = camera->view_to_clip;
        camera->sample_to_view = camera->clip_to_view;
    }

    camera->clip_to_prev_clip = std::bit_cast<daxa_f32mat4x4>(
        prev_cam.view_to_clip *
        prev_cam.world_to_view *
        cam.view_to_world *
        cam.clip_to_view);
}

void player::get_camera(Player self, Camera *camera, GpuInput const *gpu_input) {
    update_camera(&self->main);
    get_camera(self->main.cam, self->main.prev_cam, camera, gpu_input, true);
}

void player::get_observer_camera(Player self, Camera *camera, GpuInput const *gpu_input) {
    update_camera(&self->observer);
    get_camera(self->observer.cam, self->observer.prev_cam, camera, gpu_input, false);
}

auto player::should_draw_from_observer(Player self) -> bool {
    return self->viewing_observer;
}
