#ifndef ZCSR_PATHFINDING_H
#define ZCSR_PATHFINDING_H
/* zcsr_tools / common / pathfinding
 * 通用图寻路：A* / Dijkstra。调用方提供图、工作区和输出缓冲。
 * 本模块不分配内存，不拥有节点，不知道任何具体游戏语义。 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCSR_PATHFINDING_NODE_NONE UINT32_MAX

typedef enum {
    ZCSR_PATHFINDING_RESULT_FOUND = 0,
    ZCSR_PATHFINDING_RESULT_NO_PATH = 1,
    ZCSR_PATHFINDING_RESULT_INVALID_ARGUMENT = 2,
    ZCSR_PATHFINDING_RESULT_OUTPUT_TOO_SMALL = 3
} zcsr_pathfinding_result;

/* 图使用压缩邻接表：
 * neighbor_offsets 长度 node_count + 1，neighbor_offsets[0] 必须为 0；
 * neighbor_indices 与 neighbor_costs 的长度是 neighbor_offsets[node_count]。
 * neighbor_costs 可以为 NULL，此时每条边代价为 1.0。 */
typedef struct {
    uint32_t node_count;
    const uint32_t* neighbor_offsets;
    const uint32_t* neighbor_indices;
    const float* neighbor_costs;
} zcsr_pathfinding_graph;

/* 估算函数返回从 node_index 到 goal_index 的启发值。
 * 允许为 NULL；为 NULL 时使用 Dijkstra。调用方负责保证启发值可采纳。 */
typedef float (*zcsr_pathfinding_estimate_function)(
    uint32_t node_index,
    uint32_t goal_index,
    void* user_data);

/* 工作区全部由调用方提供，长度均为 node_count：
 * closed        标记已访问节点，0/1
 * parent        父节点，ZCSR_PATHFINDING_NODE_NONE 表示没有父节点
 * cost          从起点到节点的当前代价
 * estimate      从节点到目标的启发值
 * open_heap     A* 开放列表的二叉堆
 * open_position 节点在 open_heap 中的位置；ZCSR_PATHFINDING_NODE_NONE 表示不在堆中
 * open_heap_capacity 必须大于等于 node_count。 */
typedef struct {
    uint8_t* closed;
    uint32_t* parent;
    float* cost;
    float* estimate;
    uint32_t* open_heap;
    uint32_t* open_position;
    uint32_t open_heap_capacity;
} zcsr_pathfinding_workspace;

/* 查找从 start_node 到 goal_node 的路径。
 * out_path 按起点到终点顺序写入节点索引；out_path 可以为 NULL。
 * 无论是否找到路径，out_path_length 都会写入实际路径长度；容量不足时返回
 * OUTPUT_TOO_SMALL，并保留实际长度供调用方扩容后重试。 */
zcsr_pathfinding_result zcsr_pathfinding_find_path(
    const zcsr_pathfinding_graph* graph,
    zcsr_pathfinding_workspace* workspace,
    uint32_t start_node,
    uint32_t goal_node,
    zcsr_pathfinding_estimate_function estimate_function,
    void* estimate_user_data,
    uint32_t* out_path,
    uint32_t out_path_capacity,
    uint32_t* out_path_length);

/* 网格特化：四邻域、blocked 非零表示阻挡、cell_costs 可为 NULL（默认 1）。
 * workspace 数组长度均为 width * height；open_heap_capacity 必须 >= 格子数。
 * 本模块知道网格邻接，但不知道具体游戏的地图、单位或目标语义。 */
typedef struct {
    uint8_t* closed;
    uint32_t* parent;
    float* cost;
    float* estimate;
    uint32_t* open_heap;
    uint32_t* open_position;
    uint32_t open_heap_capacity;
} zcsr_pathfinding_grid_workspace;

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
    uint32_t* out_path_length);

#ifdef __cplusplus
}
#endif

#endif /* ZCSR_PATHFINDING_H */
