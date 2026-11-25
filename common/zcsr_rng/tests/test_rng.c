#include "zcsr_rng.h"

#include <assert.h>

static void test_reproducible(void)
{
    zcsr_rng a;
    zcsr_rng b;
    uint32_t i;

    zcsr_rng_seed(&a, 12345u);
    zcsr_rng_seed(&b, 12345u);
    for (i = 0; i < 64u; ++i) {
        assert(zcsr_rng_u32(&a) == zcsr_rng_u32(&b));
    }
}

static void test_float_range(void)
{
    zcsr_rng rng;
    uint32_t i;

    zcsr_rng_seed(&rng, 7u);
    for (i = 0; i < 1024u; ++i) {
        float value = zcsr_rng_float(&rng);
        assert(value >= 0.0f && value < 1.0f);
    }
}

static void test_zero_seed_not_stuck(void)
{
    zcsr_rng rng;
    zcsr_rng_seed(&rng, 0u);
    assert(zcsr_rng_u32(&rng) != 0u);
    assert(zcsr_rng_u32(&rng) != 0u);
}

int main(void)
{
    test_reproducible();
    test_float_range();
    test_zero_seed_not_stuck();
    return 0;
}
