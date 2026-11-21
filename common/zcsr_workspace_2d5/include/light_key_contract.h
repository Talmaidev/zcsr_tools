#ifndef ZCSR_LIGHT_KEY_CONTRACT_H
#define ZCSR_LIGHT_KEY_CONTRACT_H
/* Pure key-action contract for the 2.5D light demo.
 * Maps engine ZCSR_KEY_* values to demo actions and applies the state change
 * without a renderer, so the same code path can be exercised by tests.
 * This is not a second key table: it only branches on the engine enum. */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ZCSR_LIGHT_KEY_NONE = 0,
    ZCSR_LIGHT_KEY_QUALITY,
    ZCSR_LIGHT_KEY_Z_UP,
    ZCSR_LIGHT_KEY_Z_DOWN
} zcsr_light_key_action;

zcsr_light_key_action zcsr_light_key_classify(int key);
/* quality: 0=LOW, 2=HIGH (same values as zcsr_light_quality). */
void zcsr_light_key_apply(zcsr_light_key_action action, int* quality, float* light_z);

#ifdef __cplusplus
}
#endif

#endif /* ZCSR_LIGHT_KEY_CONTRACT_H */
