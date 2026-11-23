#ifndef ZCSR_FLOW_FIELD_H
#define ZCSR_FLOW_FIELD_H

/* zcsr_tools / common / flow_field
 * 通用均匀代价网格的 flow field：先构建 integration field，再写入每格方向。
 * 调用方提供全部存储；本模块不分配内存，不知道游戏规则、实体或战斗语义。 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCSR_FLOW_FIELD_UNREACHABLE UINT32_MAX
#define ZCSR_FLOW_FIELD_BLOCKED 255u

typedef struct {
    float x;
    float y;
} zcsr_flow_field_point;

typedef struct {
    uint32_t width;
    uint32_t height;
    float cell_size;

    const uint8_t* cost;      /* width * height；< 255 可通行，255 阻挡 */
    uint32_t* distance;       /* width * height；到目标的步数，UNREACHABLE 表示不可达 */
    uint32_t* queue;          /* width * height；BFS 工作区 */
    int8_t* direction_x;      /* width * height；-1/0/1 */
    int8_t* direction_y;      /* width * height；-1/0/1 */
} zcsr_flow_field;

/* 构建从 target_x/target_y 出发的 integration field 和方向场。
 * 只支持四邻域、单位代价；cost 由调用方填充。
 * 返回值表示参数是否有效；不可达格子保持 UNREACHABLE 和零方向。 */
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
    int32_t target_y);

/* 将世界坐标采样为格子方向；越界坐标夹到边界格子。
 * out_dx/out_dy 长度均为 count，调用方提供。 */
void zcsr_flow_field_sample(
    const zcsr_flow_field* field,
    const zcsr_flow_field_point* points,
    uint32_t count,
    float* out_dx,
    float* out_dy);

#ifdef __cplusplus
}
#endif

#endif /* ZCSR_FLOW_FIELD_H */
