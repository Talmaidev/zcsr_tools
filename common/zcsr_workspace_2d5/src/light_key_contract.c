#include "light_key_contract.h"

#include "zcsr/input.h"

zcsr_light_key_action zcsr_light_key_classify(int key) {
    switch (key) {
    case ZCSR_KEY_SPACE: return ZCSR_LIGHT_KEY_QUALITY;
    case ZCSR_KEY_W:     return ZCSR_LIGHT_KEY_Z_UP;
    case ZCSR_KEY_S:     return ZCSR_LIGHT_KEY_Z_DOWN;
    default:             return ZCSR_LIGHT_KEY_NONE;
    }
}

void zcsr_light_key_apply(zcsr_light_key_action action, int* quality, float* light_z) {
    if (quality && action == ZCSR_LIGHT_KEY_QUALITY) {
        *quality = (*quality == 0) ? 2 : 0;
    }
    if (light_z && action == ZCSR_LIGHT_KEY_Z_UP) {
        *light_z += 0.25f;
        if (*light_z > 8.0f) *light_z = 8.0f;
    }
    if (light_z && action == ZCSR_LIGHT_KEY_Z_DOWN) {
        *light_z -= 0.25f;
        if (*light_z < 0.5f) *light_z = 0.5f;
    }
}
