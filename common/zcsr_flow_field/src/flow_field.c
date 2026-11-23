#include "zcsr_flow_field.h"

#include <stddef.h>

bool zcsr_flow_field_build(
    zcsr_flow_field* field,
    uint32_t width,
    uint32_t height,
    float cell_size,
    const uint8_t* cost,
    uint32_t* distance,
    uint32_t* queue,
    int8_t* direction_x,
    int8_t* direction_y,
    int32_t target_x,
    int32_t target_y)
{
    static const int32_t k_dx[4] = {1, -1, 0, 0};
    static const int32_t k_dy[4] = {0, 0, 1, -1};
    size_t cells;
    size_t i;
    size_t head;
    size_t tail;
    uint32_t target;
    uint32_t y;

    if (field == NULL || cost == NULL || distance == NULL || queue == NULL ||
        direction_x == NULL || direction_y == NULL) {
        return false;
    }
    if (width == 0u || height == 0u || cell_size <= 0.0f) {
        return false;
    }
    if (target_x < 0 || target_y < 0 ||
        target_x >= (int32_t)width || target_y >= (int32_t)height) {
        return false;
    }

    cells = (size_t)width * (size_t)height;
    if (cells > (size_t)UINT32_MAX) {
        return false;
    }
    target = (uint32_t)target_y * width + (uint32_t)target_x;
    if (cost[target] >= ZCSR_FLOW_FIELD_BLOCKED) {
        return false;
    }

    field->width = width;
    field->height = height;
    field->cell_size = cell_size;
    field->cost = cost;
    field->distance = distance;
    field->queue = queue;
    field->direction_x = direction_x;
    field->direction_y = direction_y;

    for (i = 0; i < cells; ++i) {
        distance[i] = ZCSR_FLOW_FIELD_UNREACHABLE;
        direction_x[i] = 0;
        direction_y[i] = 0;
    }

    distance[target] = 0u;
    head = 0u;
    tail = 0u;
    queue[tail++] = target;

    while (head < tail) {
        uint32_t current = queue[head++];
        int32_t cx = (int32_t)(current % width);
        int32_t cy = (int32_t)(current / width);
        uint32_t current_distance = distance[current];
        int32_t d;

        for (d = 0; d < 4; ++d) {
            int32_t nx = cx + k_dx[d];
            int32_t ny = cy + k_dy[d];
            uint32_t neighbor;

            if (nx < 0 || nx >= (int32_t)width ||
                ny < 0 || ny >= (int32_t)height) {
                continue;
            }
            neighbor = (uint32_t)ny * width + (uint32_t)nx;
            if (cost[neighbor] >= ZCSR_FLOW_FIELD_BLOCKED) {
                continue;
            }
            if (distance[neighbor] != ZCSR_FLOW_FIELD_UNREACHABLE) {
                continue;
            }
            distance[neighbor] = current_distance + 1u;
            queue[tail++] = neighbor;
        }
    }

    for (y = 0u; y < height; ++y) {
        uint32_t x;
        for (x = 0u; x < width; ++x) {
            uint32_t index = y * width + x;
            uint32_t best;
            int8_t best_x = 0;
            int8_t best_y = 0;
            int32_t d;

            if (cost[index] >= ZCSR_FLOW_FIELD_BLOCKED ||
                distance[index] == ZCSR_FLOW_FIELD_UNREACHABLE ||
                index == target) {
                continue;
            }

            best = distance[index];
            for (d = 0; d < 4; ++d) {
                int32_t nx = (int32_t)x + k_dx[d];
                int32_t ny = (int32_t)y + k_dy[d];
                uint32_t neighbor;

                if (nx < 0 || nx >= (int32_t)width ||
                    ny < 0 || ny >= (int32_t)height) {
                    continue;
                }
                neighbor = (uint32_t)ny * width + (uint32_t)nx;
                if (cost[neighbor] >= ZCSR_FLOW_FIELD_BLOCKED ||
                    distance[neighbor] == ZCSR_FLOW_FIELD_UNREACHABLE) {
                    continue;
                }
                if (distance[neighbor] < best) {
                    best = distance[neighbor];
                    best_x = (int8_t)k_dx[d];
                    best_y = (int8_t)k_dy[d];
                }
            }
            direction_x[index] = best_x;
            direction_y[index] = best_y;
        }
    }

    return true;
}

void zcsr_flow_field_sample(
    const zcsr_flow_field* field,
    const zcsr_flow_field_point* points,
    uint32_t count,
    float* out_dx,
    float* out_dy)
{
    uint32_t i;

    if (field == NULL || points == NULL || out_dx == NULL || out_dy == NULL) {
        return;
    }
    if (field->width == 0u || field->height == 0u || field->cell_size <= 0.0f ||
        field->direction_x == NULL || field->direction_y == NULL) {
        return;
    }

    for (i = 0u; i < count; ++i) {
        int32_t cx = (int32_t)(points[i].x / field->cell_size);
        int32_t cy = (int32_t)(points[i].y / field->cell_size);
        uint32_t index;

        if (cx < 0) {
            cx = 0;
        } else if (cx >= (int32_t)field->width) {
            cx = (int32_t)field->width - 1;
        }
        if (cy < 0) {
            cy = 0;
        } else if (cy >= (int32_t)field->height) {
            cy = (int32_t)field->height - 1;
        }

        index = (uint32_t)cy * field->width + (uint32_t)cx;
        out_dx[i] = (float)field->direction_x[index];
        out_dy[i] = (float)field->direction_y[index];
    }
}
