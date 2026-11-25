#ifndef ZCSR_RNG_H
#define ZCSR_RNG_H

/* zcsr_tools / common / rng
 * 确定性 xorshift32 随机数；调用方持有状态，不分配内存。
 * 只提供数值序列，不包含任何游戏规则或随机事件语义。 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state;
} zcsr_rng;

void zcsr_rng_seed(zcsr_rng* rng, uint32_t seed);
uint32_t zcsr_rng_u32(zcsr_rng* rng);
float zcsr_rng_float(zcsr_rng* rng);

#ifdef __cplusplus
}
#endif

#endif /* ZCSR_RNG_H */
