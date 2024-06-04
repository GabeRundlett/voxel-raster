#pragma once

void rasterize_triangle(in vec3[3] triangle, ivec2 viewport_size, uint64_t payload) {
    const vec3 v01 = triangle[1] - triangle[0];
    const vec3 v02 = triangle[2] - triangle[0];
    const float det_xy = v01.x * v02.y - v01.y * v02.x;
    if (det_xy >= 0.0) {
        return;
    }

    const float inv_det = 1.0 / det_xy;
    vec2 grad_z = vec2(
        (v01.z * v02.y - v01.y * v02.z) * inv_det,
        (v01.x * v02.z - v01.z * v02.x) * inv_det);

    vec2 vert_0 = triangle[0].xy;
    vec2 vert_1 = triangle[1].xy;
    vec2 vert_2 = triangle[2].xy;

    const vec2 min_subpixel = min(min(vert_0, vert_1), vert_2);
    const vec2 max_subpixel = max(max(vert_0, vert_1), vert_2);

    ivec2 min_pixel = ivec2(floor((min_subpixel + (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / float(SUBPIXEL_SAMPLES))));
    ivec2 max_pixel = ivec2(floor((max_subpixel - (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / float(SUBPIXEL_SAMPLES))));

    min_pixel = max(min_pixel, ivec2(0));
    max_pixel = min(max_pixel, viewport_size.xy - 1);
    if (any(greaterThan(min_pixel, max_pixel))) {
        return;
    }

    max_pixel = min(max_pixel, min_pixel + 63);

    const vec2 edge_01 = -v01.xy;
    const vec2 edge_12 = vert_1 - vert_2;
    const vec2 edge_20 = v02.xy;

    const vec2 base_subpixel = vec2(min_pixel) * SUBPIXEL_SAMPLES + (SUBPIXEL_SAMPLES / 2);
    vert_0 -= base_subpixel;
    vert_1 -= base_subpixel;
    vert_2 -= base_subpixel;

    float hec_0 = edge_01.y * vert_0.x - edge_01.x * vert_0.y;
    float hec_1 = edge_12.y * vert_1.x - edge_12.x * vert_1.y;
    float hec_2 = edge_20.y * vert_2.x - edge_20.x * vert_2.y;

    hec_0 -= saturate(edge_01.y + saturate(1.0 - edge_01.x));
    hec_1 -= saturate(edge_12.y + saturate(1.0 - edge_12.x));
    hec_2 -= saturate(edge_20.y + saturate(1.0 - edge_20.x));

    const float z_0 = triangle[0].z - (grad_z.x * vert_0.x + grad_z.y * vert_0.y);
    grad_z *= SUBPIXEL_SAMPLES;

    float hec_y_0 = hec_0 * (1.0 / float(SUBPIXEL_SAMPLES));
    float hec_y_1 = hec_1 * (1.0 / float(SUBPIXEL_SAMPLES));
    float hec_y_2 = hec_2 * (1.0 / float(SUBPIXEL_SAMPLES));
    float z_y = z_0;

    if (subgroupAny(max_pixel.x - min_pixel.x > 4)) {
        const vec3 edge_012 = vec3(edge_01.y, edge_12.y, edge_20.y);
        const bvec3 is_open_edge = lessThan(edge_012, vec3(0.0));
        const vec3 inv_edge_012 = vec3(
            edge_012.x == 0 ? 1e8 : (1.0 / edge_012.x),
            edge_012.y == 0 ? 1e8 : (1.0 / edge_012.y),
            edge_012.z == 0 ? 1e8 : (1.0 / edge_012.z));
        int y = min_pixel.y;
        while (true) {
            const vec3 cross_x = vec3(hec_y_0, hec_y_1, hec_y_2) * inv_edge_012;
            const vec3 min_x = vec3(
                is_open_edge.x ? cross_x.x : 0.0,
                is_open_edge.y ? cross_x.y : 0.0,
                is_open_edge.z ? cross_x.z : 0.0);
            const vec3 max_x = vec3(
                is_open_edge.x ? max_pixel.x - min_pixel.x : cross_x.x,
                is_open_edge.y ? max_pixel.x - min_pixel.x : cross_x.y,
                is_open_edge.z ? max_pixel.x - min_pixel.x : cross_x.z);
            float x_0 = ceil(max(max(min_x.x, min_x.y), min_x.z));
            float x_1 = min(min(max_x.x, max_x.y), max_x.z);
            float z_x = z_y + grad_z.x * x_0;

            x_0 += min_pixel.x;
            x_1 += min_pixel.x;
            for (float x = x_0; x <= x_1; ++x) {
                write_pixel(ivec2(x, y), payload, z_x);
                z_x += grad_z.x;
            }

            if (y >= max_pixel.y) {
                break;
            }
            hec_y_0 += edge_01.x;
            hec_y_1 += edge_12.x;
            hec_y_2 += edge_20.x;
            z_y += grad_z.y;
            ++y;
        }
    } else {
        int y = min_pixel.y;
        while (true) {
            int x = min_pixel.x;
            if (min(min(hec_y_0, hec_y_1), hec_y_2) >= 0.0) {
                write_pixel(ivec2(x, y), payload, z_y);
            }

            if (x < max_pixel.x) {
                float hec_x_0 = hec_y_0 - edge_01.y;
                float hec_x_1 = hec_y_1 - edge_12.y;
                float hec_x_2 = hec_y_2 - edge_20.y;
                float z_x = z_y + grad_z.x;
                ++x;

                while (true) {
                    if (min(min(hec_x_0, hec_x_1), hec_x_2) >= 0.0) {
                        write_pixel(ivec2(x, y), payload, z_x);
                    }

                    if (x >= max_pixel.x) {
                        break;
                    }

                    hec_x_0 -= edge_01.y;
                    hec_x_1 -= edge_12.y;
                    hec_x_2 -= edge_20.y;
                    z_x += grad_z.x;
                    ++x;
                }
            }

            if (y >= max_pixel.y) {
                break;
            }

            hec_y_0 += edge_01.x;
            hec_y_1 += edge_12.x;
            hec_y_2 += edge_20.x;
            z_y += grad_z.y;
            ++y;
        }
    }
}

void rasterize_quad(in vec3[4] quad, ivec2 viewport_size, uint64_t payload) {
    rasterize_triangle(vec3[](quad[0], quad[2], quad[1]), viewport_size, payload);
    rasterize_triangle(vec3[](quad[1], quad[2], quad[3]), viewport_size, payload);
}
