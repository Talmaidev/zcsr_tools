#ifndef ZCSR_DEBUG_DRAW_H
#define ZCSR_DEBUG_DRAW_H
/* zcsr_tools / zcsr_debug_draw — backend-agnostic debug primitive command buffer.
 * No heap, fixed capacity, caller owns text pointers and the draw state. */
#include <stdbool.h>
#include <stdint.h>

#define ZCSR_DEBUG_DRAW_MAX_COMMANDS 4096u

typedef struct {
    float r, g, b, a;
} zcsr_debug_color;

typedef enum {
    ZCSR_DEBUG_CMD_LINE = 0,
    ZCSR_DEBUG_CMD_RECT,
    ZCSR_DEBUG_CMD_FILLED_RECT,
    ZCSR_DEBUG_CMD_CIRCLE,
    ZCSR_DEBUG_CMD_ARROW,
    ZCSR_DEBUG_CMD_TEXT
} zcsr_debug_cmd_type;

typedef struct {
    zcsr_debug_cmd_type type;
    float               x0, y0;
    float               x1, y1;
    float               radius;
    zcsr_debug_color    color;
    const char*         text; /* caller-owned; may be NULL for non-text commands */
} zcsr_debug_cmd;

typedef struct {
    zcsr_debug_cmd   cmds[ZCSR_DEBUG_DRAW_MAX_COMMANDS];
    uint32_t         count;
    uint32_t         overflow;
    zcsr_debug_color color;
} zcsr_debug_draw;

typedef void (*zcsr_debug_emit_fn)(const zcsr_debug_cmd* cmd, void* user);

void     zcsr_debug_draw_init(zcsr_debug_draw* d);
void     zcsr_debug_draw_begin(zcsr_debug_draw* d);
void     zcsr_debug_draw_set_color(zcsr_debug_draw* d, zcsr_debug_color c);

bool     zcsr_debug_draw_line(zcsr_debug_draw* d, float x0, float y0, float x1, float y1);
bool     zcsr_debug_draw_rect(zcsr_debug_draw* d, float x0, float y0, float x1, float y1);
bool     zcsr_debug_draw_filled_rect(zcsr_debug_draw* d, float x0, float y0, float x1, float y1);
bool     zcsr_debug_draw_circle(zcsr_debug_draw* d, float cx, float cy, float radius);
bool     zcsr_debug_draw_arrow(zcsr_debug_draw* d, float x0, float y0, float x1, float y1);
bool     zcsr_debug_draw_text(zcsr_debug_draw* d, float x, float y, const char* text);

/* Adds grid lines clipped to [x0,x1] x [y0,y1]. Returns added line count.
 * cell <= 0 or max_lines == 0 returns 0. */
uint32_t zcsr_debug_draw_grid(zcsr_debug_draw* d, float x0, float y0, float x1, float y1,
                              float cell, uint32_t max_lines);

/* Emits every recorded command through cb. Returns emitted count (0 if cb is NULL). */
uint32_t zcsr_debug_draw_flush(const zcsr_debug_draw* d, zcsr_debug_emit_fn cb, void* user);

#endif /* ZCSR_DEBUG_DRAW_H */
