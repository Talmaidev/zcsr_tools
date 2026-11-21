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
