/* Contract test for the light demo key path. It injects engine ZCSR_KEY_*
 * values into the same classify/apply functions used by light_module_key(). */
#include "light_key_contract.h"
#include "zcsr/input.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    int quality = 0; /* LOW */
    float z = 3.0f;

    assert(zcsr_light_key_classify(ZCSR_KEY_SPACE) == ZCSR_LIGHT_KEY_QUALITY);
    assert(zcsr_light_key_classify(ZCSR_KEY_W) == ZCSR_LIGHT_KEY_Z_UP);
    assert(zcsr_light_key_classify(ZCSR_KEY_S) == ZCSR_LIGHT_KEY_Z_DOWN);
    assert(zcsr_light_key_classify(0x20) == ZCSR_LIGHT_KEY_NONE);
    assert(zcsr_light_key_classify('W') == ZCSR_LIGHT_KEY_NONE);
    assert(zcsr_light_key_classify('S') == ZCSR_LIGHT_KEY_NONE);

    zcsr_light_key_apply(ZCSR_LIGHT_KEY_QUALITY, &quality, &z);
    assert(quality == 2); /* HIGH */
    zcsr_light_key_apply(ZCSR_LIGHT_KEY_QUALITY, &quality, &z);
    assert(quality == 0); /* LOW */

    zcsr_light_key_apply(ZCSR_LIGHT_KEY_Z_UP, &quality, &z);
    assert(z > 3.24f && z < 3.26f);
    zcsr_light_key_apply(ZCSR_LIGHT_KEY_Z_DOWN, &quality, &z);
    assert(z > 2.99f && z < 3.01f);

    z = 0.5f;
    zcsr_light_key_apply(ZCSR_LIGHT_KEY_Z_DOWN, &quality, &z);
    assert(z == 0.5f);
    z = 8.0f;
    zcsr_light_key_apply(ZCSR_LIGHT_KEY_Z_UP, &quality, &z);
    assert(z == 8.0f);

    printf("light_key_contract: PASS\n");
    return 0;
}
