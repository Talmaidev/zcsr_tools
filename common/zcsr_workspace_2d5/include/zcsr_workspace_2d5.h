#ifndef ZCSR_WORKSPACE_2D5_H
#define ZCSR_WORKSPACE_2D5_H
/* zcsr_tools / zcsr_workspace_2d5 — public control surface for the fixed 2.5D
 * light workspace demonstration. This is not a general scene authoring API.
 * It sets a fixed floor/wall/box demonstration, render options and camera
 * pitch. The demo frame is driven by zcsr_runtime_shell; these entry points do
 * not own a window, a scene document, entities, layers or commands. */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* effects: 0 disables decorative scene pieces; nonzero enables them.
 * view_distance: 0..100, larger = farther view (zoom out). */
bool  zcsr_light2d_module_set_scene(int effects, float view_distance);

/* shadows/light_scale_pct/bloom/ray_shadows map to the Light2D options. */
bool  zcsr_light2d_module_set_render_options(int shadows, int light_scale_pct,
                                             int bloom, int ray_shadows);

/* FOV is expressed as the equivalent 2.5D pitch; clamps to [30, 150] degrees. */
bool  zcsr_light2d_module_set_fov_deg(float fov_deg);

float zcsr_light2d_module_get_pitch_deg(void);

#ifdef __cplusplus
}
#endif

#endif /* ZCSR_WORKSPACE_2D5_H */
