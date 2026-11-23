#include "zcsr_spatial_grid.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define MAX_VISITS 16u

static uint32_t g_items[MAX_VISITS];
static uint32_t g_item_count;

static void collect_item(uint32_t item, void* user_data)
{
    (void)user_data;
    assert(g_item_count < MAX_VISITS);
    g_items[g_item_count++] = item;
}

static bool contains_item(uint32_t item)
{
    uint32_t i;
    for (i = 0; i < g_item_count; ++i) {
        if (g_items[i] == item) {
            return true;
        }
    }
    return false;
}

static void reset_visits(void)
{
    g_item_count = 0u;
}

static void test_point_and_aabb_queries(void)
{
    uint32_t cell_head[4];
    uint32_t link_item[32];
    uint32_t link_next[32];
    uint32_t item_mark[8];
    zcsr_spatial_grid grid;

    assert(zcsr_spatial_grid_init(
        &grid, 2u, 2u, 0.0f, 0.0f, 5.0f, 5.0f,
        cell_head, link_item, link_next, 32u, item_mark, 8u));

    assert(zcsr_spatial_grid_insert_aabb(&grid, 0u, 0.0f, 0.0f, 2.0f, 2.0f));
    assert(zcsr_spatial_grid_insert_aabb(&grid, 1u, 5.0f, 5.0f, 2.0f, 2.0f));
    assert(zcsr_spatial_grid_insert_aabb(&grid, 2u, 9.0f, 9.0f, 1.0f, 1.0f));
    assert(zcsr_spatial_grid_insert_aabb(&grid, 3u, 0.0f, 9.0f, 1.0f, 1.0f));
    assert(zcsr_spatial_grid_insert_aabb(&grid, 4u, 0.0f, 0.0f, 10.0f, 10.0f));

    reset_visits();
    assert(zcsr_spatial_grid_query_point(&grid, 5.5f, 5.5f, collect_item, NULL) == 3u);
    assert(contains_item(1u));
    assert(contains_item(2u));
    assert(contains_item(4u));

    reset_visits();
    assert(zcsr_spatial_grid_query_aabb(
        &grid, 0.0f, 0.0f, 10.0f, 10.0f, collect_item, NULL) == 5u);
    assert(contains_item(0u));
    assert(contains_item(1u));
    assert(contains_item(2u));
    assert(contains_item(3u));
    assert(contains_item(4u));

    reset_visits();
    assert(zcsr_spatial_grid_query_aabb(
        &grid, 0.0f, 0.0f, 2.0f, 2.0f, collect_item, NULL) == 2u);
    assert(contains_item(0u));
    assert(contains_item(4u));
}

static void test_link_capacity_is_rejected_atomically(void)
{
    uint32_t cell_head[4];
    uint32_t link_item[1];
    uint32_t link_next[1];
    zcsr_spatial_grid grid;

    assert(zcsr_spatial_grid_init(
        &grid, 2u, 2u, 0.0f, 0.0f, 5.0f, 5.0f,
        cell_head, link_item, link_next, 1u, NULL, 0u));
    assert(!zcsr_spatial_grid_insert_aabb(&grid, 9u, 0.0f, 0.0f, 10.0f, 10.0f));
    assert(grid.link_count == 0u);
}

static void test_invalid_arguments(void)
{
    uint32_t cell_head[4];
    uint32_t link_item[4];
    uint32_t link_next[4];
    zcsr_spatial_grid grid;

    assert(!zcsr_spatial_grid_init(
        NULL, 2u, 2u, 0.0f, 0.0f, 5.0f, 5.0f,
        cell_head, link_item, link_next, 4u, NULL, 0u));
    assert(!zcsr_spatial_grid_init(
        &grid, 0u, 2u, 0.0f, 0.0f, 5.0f, 5.0f,
        cell_head, link_item, link_next, 4u, NULL, 0u));
    assert(!zcsr_spatial_grid_init(
        &grid, 2u, 2u, 0.0f, 0.0f, 0.0f, 5.0f,
        cell_head, link_item, link_next, 4u, NULL, 0u));
}

int main(void)
{
    test_point_and_aabb_queries();
    test_link_capacity_is_rejected_atomically();
    test_invalid_arguments();
    return 0;
}
