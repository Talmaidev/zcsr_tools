#include "zcsr_pathfinding.h"

#include <math.h>
#include <stddef.h>

static bool zcsr_pathfinding_finite(float value) {
    return isfinite(value) != 0;
}

static void zcsr_pathfinding_heap_swap(zcsr_pathfinding_workspace* workspace,
                                       uint32_t left, uint32_t right) {
    uint32_t node = workspace->open_heap[left];
    workspace->open_heap[left] = workspace->open_heap[right];
    workspace->open_heap[right] = node;
    workspace->open_position[workspace->open_heap[left]] = left;
    workspace->open_position[workspace->open_heap[right]] = right;
}

static void zcsr_pathfinding_heap_sift_up(zcsr_pathfinding_workspace* workspace,
                                          uint32_t position) {
    while (position > 0) {
        uint32_t parent = (position - 1u) / 2u;
        if (workspace->estimate[workspace->open_heap[parent]] <=
            workspace->estimate[workspace->open_heap[position]]) break;
        zcsr_pathfinding_heap_swap(workspace, parent, position);
        position = parent;
    }
}

static void zcsr_pathfinding_heap_sift_down(zcsr_pathfinding_workspace* workspace,
                                            uint32_t position,
                                            uint32_t heap_size) {
    while (true) {
        uint32_t left = position * 2u + 1u;
        uint32_t right = left + 1u;
        uint32_t smallest = position;
        if (left < heap_size &&
            workspace->estimate[workspace->open_heap[left]] <
            workspace->estimate[workspace->open_heap[smallest]]) {
            smallest = left;
        }
        if (right < heap_size &&
            workspace->estimate[workspace->open_heap[right]] <
            workspace->estimate[workspace->open_heap[smallest]]) {
            smallest = right;
        }
        if (smallest == position) break;
        zcsr_pathfinding_heap_swap(workspace, position, smallest);
        position = smallest;
    }
}

static bool zcsr_pathfinding_heap_push(zcsr_pathfinding_workspace* workspace,
                                       uint32_t* heap_size, uint32_t node) {
    uint32_t position;
    if (*heap_size >= workspace->open_heap_capacity) return false;
    position = *heap_size;
    workspace->open_heap[position] = node;
    workspace->open_position[node] = position;
    zcsr_pathfinding_heap_sift_up(workspace, position);
    *heap_size += 1u;
    return true;
}

static uint32_t zcsr_pathfinding_heap_pop(zcsr_pathfinding_workspace* workspace,
                                          uint32_t* heap_size) {
    uint32_t root;
    if (*heap_size == 0) return ZCSR_PATHFINDING_NODE_NONE;
    root = workspace->open_heap[0];
    *heap_size -= 1u;
    if (*heap_size > 0) {
        workspace->open_heap[0] = workspace->open_heap[*heap_size];
        workspace->open_position[workspace->open_heap[0]] = 0;
        zcsr_pathfinding_heap_sift_down(workspace, 0, *heap_size);
    }
    workspace->open_position[root] = ZCSR_PATHFINDING_NODE_NONE;
    return root;
}

static void zcsr_pathfinding_heap_decrease_key(zcsr_pathfinding_workspace* workspace,
                                                uint32_t node) {
    uint32_t position = workspace->open_position[node];
    if (position != ZCSR_PATHFINDING_NODE_NONE)
        zcsr_pathfinding_heap_sift_up(workspace, position);
}

static bool zcsr_pathfinding_graph_valid(const zcsr_pathfinding_graph* graph) {
    uint32_t index;
    uint32_t total_edges;
    if (graph == NULL || graph->node_count == 0 || graph->neighbor_offsets == NULL)
        return false;
    if (graph->neighbor_offsets[0] != 0) return false;
    for (index = 0; index < graph->node_count; ++index) {
        if (graph->neighbor_offsets[index + 1] < graph->neighbor_offsets[index])
            return false;
    }
    total_edges = graph->neighbor_offsets[graph->node_count];
    return total_edges == 0 || graph->neighbor_indices != NULL;
}

static bool zcsr_pathfinding_workspace_valid(
    const zcsr_pathfinding_graph* graph,
    const zcsr_pathfinding_workspace* workspace) {
    if (workspace == NULL) return false;
    if (workspace->closed == NULL || workspace->parent == NULL ||
        workspace->cost == NULL || workspace->estimate == NULL ||
        workspace->open_heap == NULL || workspace->open_position == NULL)
        return false;
    return workspace->open_heap_capacity >= graph->node_count;
}

static bool zcsr_pathfinding_estimate_value(
    zcsr_pathfinding_estimate_function estimate_function,
    uint32_t node,
    uint32_t goal,
    void* user_data,
    float* out_value) {
    float value = 0.0f;
    if (estimate_function != NULL)
        value = estimate_function(node, goal, user_data);
    if (!zcsr_pathfinding_finite(value) || value < 0.0f) return false;
    *out_value = value;
    return true;
}

zcsr_pathfinding_result zcsr_pathfinding_find_path(
    const zcsr_pathfinding_graph* graph,
    zcsr_pathfinding_workspace* workspace,
    uint32_t start_node,
    uint32_t goal_node,
    zcsr_pathfinding_estimate_function estimate_function,
    void* estimate_user_data,
    uint32_t* out_path,
    uint32_t out_path_capacity,
    uint32_t* out_path_length) {
    uint32_t index;
    uint32_t heap_size = 0;
    uint32_t current;
    uint32_t path_length;
    uint32_t node;
    float start_estimate;

    if (out_path_length == NULL) return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
    *out_path_length = 0;
    if (!zcsr_pathfinding_graph_valid(graph) ||
        !zcsr_pathfinding_workspace_valid(graph, workspace) ||
        start_node >= graph->node_count || goal_node >= graph->node_count ||
        (out_path == NULL && out_path_capacity != 0) ||
        (out_path != NULL && out_path_capacity == 0))
        return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;

    if (start_node == goal_node) {
        *out_path_length = 1;
        if (out_path == NULL || out_path_capacity < 1)
            return ZCSR_PATHFINDING_RESULT_OUTPUT_TOO_SMALL;
        out_path[0] = start_node;
        return ZCSR_PATHFINDING_RESULT_FOUND;
    }

    for (index = 0; index < graph->node_count; ++index) {
        workspace->closed[index] = 0;
        workspace->parent[index] = ZCSR_PATHFINDING_NODE_NONE;
        workspace->cost[index] = INFINITY;
        workspace->estimate[index] = INFINITY;
        workspace->open_position[index] = ZCSR_PATHFINDING_NODE_NONE;
    }

    if (!zcsr_pathfinding_estimate_value(estimate_function, start_node,
                                         goal_node, estimate_user_data,
                                         &start_estimate))
        return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
    workspace->cost[start_node] = 0.0f;
    workspace->estimate[start_node] = start_estimate;
    if (!zcsr_pathfinding_heap_push(workspace, &heap_size, start_node))
        return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;

    while (heap_size > 0) {
        uint32_t edge_start;
        uint32_t edge_end;
        float current_cost;
        current = zcsr_pathfinding_heap_pop(workspace, &heap_size);
        if (current == ZCSR_PATHFINDING_NODE_NONE ||
            workspace->closed[current]) continue;
        workspace->closed[current] = 1;
        if (current == goal_node) break;

        current_cost = workspace->cost[current];
        edge_start = graph->neighbor_offsets[current];
        edge_end = graph->neighbor_offsets[current + 1];
        for (index = edge_start; index < edge_end; ++index) {
            uint32_t neighbor = graph->neighbor_indices[index];
            float edge_cost = 1.0f;
            float new_cost;
            float heuristic;
            if (neighbor >= graph->node_count) return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
            if (workspace->closed[neighbor]) continue;
            if (graph->neighbor_costs != NULL) edge_cost = graph->neighbor_costs[index];
            if (!zcsr_pathfinding_finite(edge_cost) || edge_cost < 0.0f)
                return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
            new_cost = current_cost + edge_cost;
            if (new_cost >= workspace->cost[neighbor]) continue;
            if (!zcsr_pathfinding_estimate_value(estimate_function, neighbor,
                                                 goal_node, estimate_user_data,
                                                 &heuristic))
                return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
            workspace->parent[neighbor] = current;
            workspace->cost[neighbor] = new_cost;
            workspace->estimate[neighbor] = new_cost + heuristic;
            if (workspace->open_position[neighbor] == ZCSR_PATHFINDING_NODE_NONE) {
                if (!zcsr_pathfinding_heap_push(workspace, &heap_size, neighbor))
                    return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
            } else {
                zcsr_pathfinding_heap_decrease_key(workspace, neighbor);
            }
        }
    }

    if (!workspace->closed[goal_node]) return ZCSR_PATHFINDING_RESULT_NO_PATH;

    path_length = 0;
    node = goal_node;
    while (node != ZCSR_PATHFINDING_NODE_NONE) {
        ++path_length;
        if (path_length > graph->node_count)
            return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
        if (node == start_node) break;
        node = workspace->parent[node];
    }
    if (node != start_node) return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;

    *out_path_length = path_length;
    if (out_path == NULL || out_path_capacity < path_length)
        return ZCSR_PATHFINDING_RESULT_OUTPUT_TOO_SMALL;

    node = goal_node;
    for (index = path_length; index > 0; --index) {
        out_path[index - 1u] = node;
        node = workspace->parent[node];
    }
    return ZCSR_PATHFINDING_RESULT_FOUND;
}

/* ---------------- grid-specialized A* ---------------- */

static void zcsr_grid_heap_swap(
    zcsr_pathfinding_grid_workspace* workspace,
    uint32_t left,
    uint32_t right)
{
    uint32_t node = workspace->open_heap[left];
    workspace->open_heap[left] = workspace->open_heap[right];
    workspace->open_heap[right] = node;
    workspace->open_position[workspace->open_heap[left]] = left;
    workspace->open_position[workspace->open_heap[right]] = right;
}

static void zcsr_grid_heap_sift_up(
    zcsr_pathfinding_grid_workspace* workspace,
    uint32_t position)
{
    while (position > 0u) {
        uint32_t parent = (position - 1u) / 2u;
        if (workspace->estimate[workspace->open_heap[parent]] <=
            workspace->estimate[workspace->open_heap[position]]) {
            break;
        }
        zcsr_grid_heap_swap(workspace, parent, position);
        position = parent;
    }
}

static void zcsr_grid_heap_sift_down(
    zcsr_pathfinding_grid_workspace* workspace,
    uint32_t position,
    uint32_t heap_size)
{
    for (;;) {
        uint32_t left = position * 2u + 1u;
        uint32_t right = left + 1u;
        uint32_t smallest = position;

        if (left < heap_size &&
            workspace->estimate[workspace->open_heap[left]] <
            workspace->estimate[workspace->open_heap[smallest]]) {
            smallest = left;
        }
        if (right < heap_size &&
            workspace->estimate[workspace->open_heap[right]] <
            workspace->estimate[workspace->open_heap[smallest]]) {
            smallest = right;
        }
        if (smallest == position) {
            break;
        }
        zcsr_grid_heap_swap(workspace, position, smallest);
        position = smallest;
    }
}

static bool zcsr_grid_heap_push(
    zcsr_pathfinding_grid_workspace* workspace,
    uint32_t* heap_size,
    uint32_t node)
{
    uint32_t position;

    if (*heap_size >= workspace->open_heap_capacity) {
        return false;
    }
    position = *heap_size;
    workspace->open_heap[position] = node;
    workspace->open_position[node] = position;
    zcsr_grid_heap_sift_up(workspace, position);
    *heap_size += 1u;
    return true;
}

static uint32_t zcsr_grid_heap_pop(
    zcsr_pathfinding_grid_workspace* workspace,
    uint32_t* heap_size)
{
    uint32_t root;

    if (*heap_size == 0u) {
        return ZCSR_PATHFINDING_NODE_NONE;
    }
    root = workspace->open_heap[0];
    *heap_size -= 1u;
    if (*heap_size > 0u) {
        workspace->open_heap[0] = workspace->open_heap[*heap_size];
        workspace->open_position[workspace->open_heap[0]] = 0u;
        zcsr_grid_heap_sift_down(workspace, 0u, *heap_size);
    }
    workspace->open_position[root] = ZCSR_PATHFINDING_NODE_NONE;
    return root;
}

static void zcsr_grid_heap_decrease_key(
    zcsr_pathfinding_grid_workspace* workspace,
    uint32_t node)
{
    zcsr_grid_heap_sift_up(workspace, workspace->open_position[node]);
}

static uint32_t zcsr_grid_heuristic(
    uint32_t node,
    uint32_t goal_node,
    uint32_t width)
{
    uint32_t node_x = node % width;
    uint32_t node_y = node / width;
    uint32_t goal_x = goal_node % width;
    uint32_t goal_y = goal_node / width;
    uint32_t dx = node_x > goal_x ? node_x - goal_x : goal_x - node_x;
    uint32_t dy = node_y > goal_y ? node_y - goal_y : goal_y - node_y;
    return dx + dy;
}

static bool zcsr_grid_workspace_valid(
    const zcsr_pathfinding_grid_workspace* workspace,
    uint32_t cells)
{
    if (workspace == NULL || workspace->closed == NULL ||
        workspace->parent == NULL || workspace->cost == NULL ||
        workspace->estimate == NULL || workspace->open_heap == NULL ||
        workspace->open_position == NULL) {
        return false;
    }
    return workspace->open_heap_capacity >= cells;
}

static bool zcsr_grid_cell_cost_valid(
    const uint8_t* blocked,
    const float* cell_costs,
    uint32_t cells)
{
    uint32_t index;
    if (cell_costs == NULL) {
        return true;
    }
    for (index = 0; index < cells; ++index) {
        if (blocked[index]) {
            continue;
        }
        if (!zcsr_pathfinding_finite(cell_costs[index]) || cell_costs[index] < 1.0f) {
            return false;
        }
    }
    return true;
}

zcsr_pathfinding_result zcsr_pathfinding_find_grid_path(
    uint32_t width,
    uint32_t height,
    const uint8_t* blocked,
    const float* cell_costs,
    zcsr_pathfinding_grid_workspace* workspace,
    uint32_t start_x,
    uint32_t start_y,
    uint32_t goal_x,
    uint32_t goal_y,
    uint32_t* out_path,
    uint32_t out_path_capacity,
    uint32_t* out_path_length)
{
    static const int32_t k_dx[4] = {1, -1, 0, 0};
    static const int32_t k_dy[4] = {0, 0, 1, -1};
    uint32_t cells;
    uint32_t start_node;
    uint32_t goal_node;
    uint32_t heap_size = 0u;
    uint32_t index;
    uint32_t node;
    uint32_t path_length;

    if (out_path_length == NULL || blocked == NULL || width == 0u || height == 0u) {
        return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
    }
    if ((uint64_t)width * (uint64_t)height > (uint64_t)UINT32_MAX) {
        return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
    }
    cells = width * height;
    if (!zcsr_grid_workspace_valid(workspace, cells) ||
        !zcsr_grid_cell_cost_valid(blocked, cell_costs, cells)) {
        return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
    }
    if (start_x >= width || start_y >= height ||
        goal_x >= width || goal_y >= height) {
        return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
    }

    start_node = start_y * width + start_x;
    goal_node = goal_y * width + goal_x;
    *out_path_length = 0u;
    if (blocked[start_node] || blocked[goal_node]) {
        return ZCSR_PATHFINDING_RESULT_NO_PATH;
    }
    if (start_node == goal_node) {
        *out_path_length = 1u;
        if (out_path == NULL || out_path_capacity < 1u) {
            return ZCSR_PATHFINDING_RESULT_OUTPUT_TOO_SMALL;
        }
        out_path[0] = start_node;
        return ZCSR_PATHFINDING_RESULT_FOUND;
    }

    for (index = 0; index < cells; ++index) {
        workspace->closed[index] = 0u;
        workspace->parent[index] = ZCSR_PATHFINDING_NODE_NONE;
        workspace->cost[index] = INFINITY;
        workspace->estimate[index] = INFINITY;
        workspace->open_position[index] = ZCSR_PATHFINDING_NODE_NONE;
    }

    workspace->cost[start_node] = 0.0f;
    workspace->estimate[start_node] =
        (float)zcsr_grid_heuristic(start_node, goal_node, width);
    if (!zcsr_grid_heap_push(workspace, &heap_size, start_node)) {
        return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
    }

    while (heap_size > 0u) {
        uint32_t current = zcsr_grid_heap_pop(workspace, &heap_size);
        int32_t current_x;
        int32_t current_y;
        int32_t direction;

        if (current == ZCSR_PATHFINDING_NODE_NONE || workspace->closed[current]) {
            continue;
        }
        workspace->closed[current] = 1u;
        if (current == goal_node) {
            break;
        }

        current_x = (int32_t)(current % width);
        current_y = (int32_t)(current / width);
        for (direction = 0; direction < 4; ++direction) {
            int32_t next_x = current_x + k_dx[direction];
            int32_t next_y = current_y + k_dy[direction];
            uint32_t neighbor;
            float neighbor_cost;
            float new_cost;

            if (next_x < 0 || next_x >= (int32_t)width ||
                next_y < 0 || next_y >= (int32_t)height) {
                continue;
            }
            neighbor = (uint32_t)next_y * width + (uint32_t)next_x;
            if (blocked[neighbor] || workspace->closed[neighbor]) {
                continue;
            }
            neighbor_cost = cell_costs != NULL ? cell_costs[neighbor] : 1.0f;
            new_cost = workspace->cost[current] + neighbor_cost;
            if (new_cost >= workspace->cost[neighbor]) {
                continue;
            }
            workspace->parent[neighbor] = current;
            workspace->cost[neighbor] = new_cost;
            workspace->estimate[neighbor] =
                new_cost + (float)zcsr_grid_heuristic(neighbor, goal_node, width);
            if (workspace->open_position[neighbor] == ZCSR_PATHFINDING_NODE_NONE) {
                if (!zcsr_grid_heap_push(workspace, &heap_size, neighbor)) {
                    return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
                }
            } else {
                zcsr_grid_heap_decrease_key(workspace, neighbor);
            }
        }
    }

    if (!workspace->closed[goal_node]) {
        return ZCSR_PATHFINDING_RESULT_NO_PATH;
    }

    path_length = 0u;
    node = goal_node;
    while (node != ZCSR_PATHFINDING_NODE_NONE) {
        ++path_length;
        if (path_length > cells) {
            return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
        }
        if (node == start_node) {
            break;
        }
        node = workspace->parent[node];
    }
    if (node != start_node) {
        return ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT;
    }

    *out_path_length = path_length;
    if (out_path == NULL || out_path_capacity < path_length) {
        return ZCSR_PATHFINDING_RESULT_OUTPUT_TOO_SMALL;
    }

    node = goal_node;
    index = path_length;
    while (index > 0u) {
        out_path[--index] = node;
        if (node == start_node) {
            break;
        }
        node = workspace->parent[node];
    }
    return ZCSR_PATHFINDING_RESULT_FOUND;
}
