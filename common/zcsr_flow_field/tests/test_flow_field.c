#include "zcsr_flow_field.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void fill_open(uint8_t* cost, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; ++i) {
        cost[i] = 1u;
    }
}

static void test_open_field(void)
{
    uint8_t cost[25];
    uint32_t distance[25];
    uint32_t queue[25];
    int8_t direction_x[25];
    int8_t direction_y[25];
    zcsr_flow_field field;
    zcsr_flow_field_point points[2];
    float out_dx[2];
    float out_dy[2];

    fill_open(cost, 25u);
    assert(zcsr_flow_field_build(
        &field, 5u, 5u, 1.0f, cost, distance, queue,
        direction_x, direction_y, 4, 4));
    assert(field.width == 5u);
    assert(field.height == 5u);
    assert(field.cell_size == 1.0f);
    assert(distance[4u * 5u + 4u] == 0u);
    assert(distance[0] != ZCSR_FLOW_FIELD_UNREACHABLE);

    points[0].x = 0.5f;
    points[0].y = 0.5f;
    points[1].x = 4.5f;
    points[1].y = 4.5f;
    zcsr_flow_field_sample(&field, points, 2u, out_dx, out_dy);

    assert(out_dx[0] != 0.0f || out_dy[0] != 0.0f);
    assert(out_dx[1] == 0.0f);
    assert(out_dy[1] == 0.0f);
}

static void test_blocked_region_is_unreachable(void)
{
    uint8_t cost[25];
    uint32_t distance[25];
    uint32_t queue[25];
    int8_t direction_x[25];
    int8_t direction_y[25];
    zcsr_flow_field field;
    zcsr_flow_field_point point;
    float out_dx;
    float out_dy;
    uint32_t y;

    fill_open(cost, 25u);
    for (y = 0; y < 5u; ++y) {
        cost[y * 5u + 1u] = ZCSR_FLOW_FIELD_BLOCKED;
    }

    assert(zcsr_flow_field_build(
        &field, 5u, 5u, 1.0f, cost, distance, queue,
        direction_x, direction_y, 4, 4));
    assert(distance[4u * 5u + 4u] == 0u);
    assert(distance[0] == ZCSR_FLOW_FIELD_UNREACHABLE);
    assert(direction_x[0] == 0);
    assert(direction_y[0] == 0);

    point.x = 0.5f;
    point.y = 0.5f;
    zcsr_flow_field_sample(&field, &point, 1u, &out_dx, &out_dy);
    assert(out_dx == 0.0f);
    assert(out_dy == 0.0f);
}

static void test_invalid_arguments(void)
{
    uint8_t cost[25];
    uint32_t distance[25];
    uint32_t queue[25];
    int8_t direction_x[25];
    int8_t direction_y[25];
    zcsr_flow_field field;

    fill_open(cost, 25u);
    assert(!zcsr_flow_field_build(
        NULL, 5u, 5u, 1.0f, cost, distance, queue,
        direction_x, direction_y, 4, 4));
    assert(!zcsr_flow_field_build(
        &field, 5u, 5u, 1.0f, NULL, distance, queue,
        direction_x, direction_y, 4, 4));
    assert(!zcsr_flow_field_build(
        &field, 5u, 5u, 1.0f, cost, distance, queue,
        direction_x, direction_y, 5, 4));
}

int main(void)
{
    test_open_field();
    test_blocked_region_is_unreachable();
    test_invalid_arguments();
    return 0;
}
