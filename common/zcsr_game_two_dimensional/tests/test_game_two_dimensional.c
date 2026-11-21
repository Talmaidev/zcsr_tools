#include "zcsr_game_two_dimensional.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int near_enough(float actual, float expected) {
    float difference = actual - expected;
    if (difference < 0.0f) difference = -difference;
    return difference <= 0.0001f;
}

static int expect(int condition, const char* label) {
    if (!condition) printf("FAIL: %s\n", label);
    return condition ? 0 : 1;
}

int main(void) {
    zcsr_game_two_dimensional_grid grid;
    zcsr_game_two_dimensional_cell cell;
    zcsr_game_two_dimensional_point point;
    uint32_t occupied_kind[3u * 4u];
    int fails = 0;

    zcsr_game_two_dimensional_grid_initialize(
        &grid, 3u, 4u, 10.0f, 20.0f, 5.0f, 6.0f);
    fails += expect(zcsr_game_two_dimensional_grid_bind_occupancy(
                        &grid, occupied_kind, 4u,
                        ZCSR_GAME_TWO_DIMENSIONAL_EMPTY_KIND),
                    "bind occupancy");
    fails += expect(grid.rows == 3u && grid.columns == 4u, "grid dimensions");
    fails += expect(zcsr_game_two_dimensional_grid_is_inside(
                        &grid, 2u, 3u), "inside");
    fails += expect(!zcsr_game_two_dimensional_grid_is_inside(
                        &grid, 3u, 3u), "row out");
    fails += expect(!zcsr_game_two_dimensional_grid_is_inside(
                        &grid, 2u, 4u), "column out");

    fails += expect(zcsr_game_two_dimensional_grid_world_to_cell(
                        &grid, 27.5f, 35.0f, &cell),
                    "world to cell");
    fails += expect(cell.row == 2u && cell.column == 3u, "cell values");
    fails += expect(zcsr_game_two_dimensional_grid_cell_to_world_center(
                        &grid, 2u, 3u, &point),
                    "cell to world");
    fails += expect(near_enough(point.x, 27.5f), "center x");
    fails += expect(near_enough(point.y, 35.0f), "center y");
    fails += expect(!zcsr_game_two_dimensional_grid_world_to_cell(
                        &grid, 9.0f, 20.0f, &cell),
                    "left of origin rejected");
    fails += expect(!zcsr_game_two_dimensional_grid_world_to_cell(
                        &grid, 30.0f, 20.0f, &cell),
                    "right edge rejected");
    fails += expect(!zcsr_game_two_dimensional_grid_world_to_cell(
                        &grid, 10.0f, 38.0f, &cell),
                    "bottom edge rejected");

    fails += expect(zcsr_game_two_dimensional_grid_occupied_kind_at(
                        &grid, 0u, 0u) ==
                        ZCSR_GAME_TWO_DIMENSIONAL_EMPTY_KIND,
                    "empty initial");
    fails += expect(zcsr_game_two_dimensional_grid_try_occupy(
                        &grid, 1u, 2u, 7u),
                    "occupy");
    fails += expect(zcsr_game_two_dimensional_grid_occupied_kind_at(
                        &grid, 1u, 2u) == 7u,
                    "occupied kind");
    fails += expect(!zcsr_game_two_dimensional_grid_try_occupy(
                        &grid, 1u, 2u, 8u),
                    "double occupy rejected");
    fails += expect(zcsr_game_two_dimensional_grid_release(
                        &grid, 1u, 2u),
                    "release");
    fails += expect(zcsr_game_two_dimensional_grid_occupied_kind_at(
                        &grid, 1u, 2u) ==
                        ZCSR_GAME_TWO_DIMENSIONAL_EMPTY_KIND,
                    "empty after release");
    fails += expect(!zcsr_game_two_dimensional_grid_release(
                        &grid, 1u, 2u),
                    "release empty rejected");

    {
        zcsr_game_two_dimensional_grid grid_with_custom_empty;
        uint32_t occupied_with_custom_empty[3u * 4u];
        zcsr_game_two_dimensional_grid_initialize(
            &grid_with_custom_empty, 3u, 4u, 10.0f, 20.0f, 5.0f, 6.0f);
        fails += expect(zcsr_game_two_dimensional_grid_bind_occupancy(
                            &grid_with_custom_empty,
                            occupied_with_custom_empty, 4u, UINT32_MAX),
                        "bind custom empty");
        fails += expect(zcsr_game_two_dimensional_grid_occupied_kind_at(
                            &grid_with_custom_empty, 0u, 0u) == UINT32_MAX,
                        "custom empty initial");
        fails += expect(zcsr_game_two_dimensional_grid_try_occupy(
                            &grid_with_custom_empty, 0u, 0u, 42u),
                        "custom empty occupy");
        fails += expect(zcsr_game_two_dimensional_grid_occupied_kind_at(
                            &grid_with_custom_empty, 0u, 0u) == 42u,
                        "custom empty occupied value");
    }

    if (fails == 0) printf("test_game_two_dimensional: PASS\n");
    return fails ? 1 : 0;
}
