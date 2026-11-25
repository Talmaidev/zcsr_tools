#include "zcsr_pathfinding.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_NODE_COUNT 16u
#define TEST_EDGE_CAPACITY 64u

typedef struct {
    uint32_t offsets[TEST_NODE_COUNT + 1u];
    uint32_t indices[TEST_EDGE_CAPACITY];
    float costs[TEST_EDGE_CAPACITY];
} test_graph;

static int expect(int condition, const char* label) {
    if (!condition) printf("FAIL: %s\n", label);
    return condition ? 0 : 1;
}

static void build_grid_graph(test_graph* graph, const uint8_t* blocked,
                             uint32_t rows, uint32_t columns) {
    uint32_t edge_count = 0;
    uint32_t row;
    uint32_t column;
    for (row = 0; row < rows; ++row) {
        for (column = 0; column < columns; ++column) {
            uint32_t node = row * columns + column;
            graph->offsets[node] = edge_count;
            if (blocked[node]) continue;
            if (row > 0) {
                uint32_t neighbor = (row - 1u) * columns + column;
                if (!blocked[neighbor]) {
                    graph->indices[edge_count] = neighbor;
                    graph->costs[edge_count] = 1.0f;
                    edge_count++;
                }
            }
            if (row + 1u < rows) {
                uint32_t neighbor = (row + 1u) * columns + column;
                if (!blocked[neighbor]) {
                    graph->indices[edge_count] = neighbor;
                    graph->costs[edge_count] = 1.0f;
                    edge_count++;
                }
            }
            if (column > 0) {
                uint32_t neighbor = row * columns + (column - 1u);
                if (!blocked[neighbor]) {
                    graph->indices[edge_count] = neighbor;
                    graph->costs[edge_count] = 1.0f;
                    edge_count++;
                }
            }
            if (column + 1u < columns) {
                uint32_t neighbor = row * columns + (column + 1u);
                if (!blocked[neighbor]) {
                    graph->indices[edge_count] = neighbor;
                    graph->costs[edge_count] = 1.0f;
                    edge_count++;
                }
            }
        }
    }
    graph->offsets[TEST_NODE_COUNT] = edge_count;
}

static float manhattan_estimate(uint32_t node, uint32_t goal, void* user_data) {
    uint32_t columns = *(const uint32_t*)user_data;
    uint32_t node_row = node / columns;
    uint32_t node_column = node % columns;
    uint32_t goal_row = goal / columns;
    uint32_t goal_column = goal % columns;
    float row_distance = (float)(node_row > goal_row ? node_row - goal_row : goal_row - node_row);
    float column_distance =
        (float)(node_column > goal_column ? node_column - goal_column : goal_column - node_column);
    return row_distance + column_distance;
}

static int path_is_connected(const test_graph* graph, const uint32_t* path,
                             uint32_t path_length) {
    uint32_t index;
    for (index = 1; index < path_length; ++index) {
        uint32_t from = path[index - 1u];
        uint32_t to = path[index];
        uint32_t edge;
        int found = 0;
        for (edge = graph->offsets[from]; edge < graph->offsets[from + 1u]; ++edge) {
            if (graph->indices[edge] == to) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
    }
    return 1;
}

static int test_grid_path(void)
{
    enum { WIDTH = 32, HEIGHT = 32, CELLS = WIDTH * HEIGHT };
    uint8_t blocked[CELLS];
    uint8_t closed[CELLS];
    uint32_t parent[CELLS];
    float cost[CELLS];
    float estimate[CELLS];
    uint32_t open_heap[CELLS];
    uint32_t open_position[CELLS];
    uint32_t path[CELLS];
    uint32_t path_length = 0u;
    zcsr_pathfinding_grid_workspace workspace;
    zcsr_pathfinding_result result;
    int fails = 0;
    uint32_t index;

    memset(blocked, 0, sizeof(blocked));
    workspace.closed = closed;
    workspace.parent = parent;
    workspace.cost = cost;
    workspace.estimate = estimate;
    workspace.open_heap = open_heap;
    workspace.open_position = open_position;
    workspace.open_heap_capacity = CELLS;

    result = zcsr_pathfinding_find_grid_path(
        WIDTH, HEIGHT, blocked, NULL, &workspace,
        0u, 0u, WIDTH - 1u, HEIGHT - 1u, path, CELLS, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_FOUND, "grid path found");
    fails += expect(path_length == 63u, "grid path shortest length");
    fails += expect(path[0] == 0u && path[path_length - 1u] == CELLS - 1u,
                    "grid path endpoints");

    result = zcsr_pathfinding_find_grid_path(
        WIDTH, HEIGHT, blocked, NULL, &workspace,
        0u, 0u, WIDTH - 1u, HEIGHT - 1u, path, 1u, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_OUTPUT_TOO_SMALL,
                    "grid small output rejected");
    fails += expect(path_length == 63u, "grid small output keeps length");

    result = zcsr_pathfinding_find_grid_path(
        WIDTH, HEIGHT, blocked, NULL, &workspace,
        5u, 5u, 5u, 5u, path, CELLS, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_FOUND, "grid same cell");
    fails += expect(path_length == 1u && path[0] == 5u * WIDTH + 5u,
                    "grid same cell path");

    for (index = 0; index < WIDTH; ++index) {
        blocked[10u * WIDTH + index] = 1u;
    }
    result = zcsr_pathfinding_find_grid_path(
        WIDTH, HEIGHT, blocked, NULL, &workspace,
        0u, 0u, WIDTH - 1u, HEIGHT - 1u, path, CELLS, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_NO_PATH,
                    "grid blocked row no path");
    fails += expect(path_length == 0u, "grid no path length");

    blocked[0] = 1u;
    result = zcsr_pathfinding_find_grid_path(
        WIDTH, HEIGHT, blocked, NULL, &workspace,
        0u, 0u, WIDTH - 1u, HEIGHT - 1u, path, CELLS, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_NO_PATH,
                    "grid blocked start no path");

    return fails;
}

int main(void) {
    test_graph graph;
    zcsr_pathfinding_graph graph_view;
    uint8_t closed[TEST_NODE_COUNT];
    uint32_t parent[TEST_NODE_COUNT];
    float cost[TEST_NODE_COUNT];
    float estimate[TEST_NODE_COUNT];
    uint32_t open_heap[TEST_NODE_COUNT];
    uint32_t open_position[TEST_NODE_COUNT];
    zcsr_pathfinding_workspace workspace;
    uint8_t blocked[TEST_NODE_COUNT];
    uint32_t path[TEST_NODE_COUNT];
    uint32_t path_length = 0;
    uint32_t columns = 4u;
    uint32_t index;
    int fails = 0;
    zcsr_pathfinding_result result;

    for (index = 0; index < TEST_NODE_COUNT; ++index) blocked[index] = 0;
    build_grid_graph(&graph, blocked, 4u, 4u);
    graph_view.node_count = TEST_NODE_COUNT;
    graph_view.neighbor_offsets = graph.offsets;
    graph_view.neighbor_indices = graph.indices;
    graph_view.neighbor_costs = graph.costs;
    workspace.closed = closed;
    workspace.parent = parent;
    workspace.cost = cost;
    workspace.estimate = estimate;
    workspace.open_heap = open_heap;
    workspace.open_position = open_position;
    workspace.open_heap_capacity = TEST_NODE_COUNT;

    result = zcsr_pathfinding_find_path(
        &graph_view, &workspace, 0u, 15u, manhattan_estimate, &columns,
        path, TEST_NODE_COUNT, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_FOUND, "grid path found");
    fails += expect(path_length > 0u && path[0] == 0u &&
                    path[path_length - 1u] == 15u, "grid path endpoints");
    fails += expect(path_is_connected(&graph, path, path_length), "grid path connected");

    result = zcsr_pathfinding_find_path(
        &graph_view, &workspace, 0u, 15u, NULL, NULL,
        path, TEST_NODE_COUNT, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_FOUND, "dijkstra path found");

    result = zcsr_pathfinding_find_path(
        &graph_view, &workspace, 0u, 0u, manhattan_estimate, &columns,
        path, TEST_NODE_COUNT, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_FOUND, "same node found");
    fails += expect(path_length == 1u && path[0] == 0u, "same node path");

    result = zcsr_pathfinding_find_path(
        &graph_view, &workspace, 0u, 15u, manhattan_estimate, &columns,
        NULL, 0u, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_OUTPUT_TOO_SMALL,
                    "small output rejected");
    fails += expect(path_length > 0u, "small output keeps path length");

    for (index = 0; index < 4u; ++index) blocked[4u + index] = 1;
    build_grid_graph(&graph, blocked, 4u, 4u);
    result = zcsr_pathfinding_find_path(
        &graph_view, &workspace, 0u, 15u, manhattan_estimate, &columns,
        path, TEST_NODE_COUNT, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_NO_PATH, "blocked graph no path");
    fails += expect(path_length == 0u, "no path length");

    for (index = 0; index < TEST_NODE_COUNT; ++index) blocked[index] = 0;
    build_grid_graph(&graph, blocked, 4u, 4u);
    workspace.open_heap_capacity = 3u;
    result = zcsr_pathfinding_find_path(
        &graph_view, &workspace, 0u, 15u, manhattan_estimate, &columns,
        path, TEST_NODE_COUNT, &path_length);
    fails += expect(result == ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT,
                    "small workspace rejected");

    fails += test_grid_path();

    if (fails == 0) printf("test_pathfinding: PASS\n");
    return fails ? 1 : 0;
}
