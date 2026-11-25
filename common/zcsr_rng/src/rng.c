#include "zcsr_rng.h"

void zcsr_rng_seed(zcsr_rng* rng, uint32_t seed)
{
    if (rng == 0) {
        return;
    }
    rng->state = seed;
}

uint32_t zcsr_rng_u32(zcsr_rng* rng)
{
    uint32_t x;

    if (rng == 0) {
        return 0u;
    }
    x = rng->state;
    if (x == 0u) {
        x = 0x9E3779B9u;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

float zcsr_rng_float(zcsr_rng* rng)
{
    /* 24 bits of mantissa, [0, 1). */
    return (float)(zcsr_rng_u32(rng) >> 8) * (1.0f / 16777216.0f);
}
