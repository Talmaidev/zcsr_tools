#include "zcsr_debug_draw.h"

#include <stddef.h>
#include <string.h>

static zcsr_debug_cmd* push_cmd(zcsr_debug_draw* d, zcsr_debug_cmd_type type) {
    zcsr_debug_cmd* c;
    if (!d) return NULL;
    if (d->count >= ZCSR_DEBUG_DRAW_MAX_COMMANDS) {
        ++d->overflow;
        return NULL;
    }
    c = &d->cmds[d->count++];
    memset(c, 0, sizeof *c);
    c->type = type;
    c->color = d->color;
    return c;
}

void zcsr_debug_draw_init(zcsr_debug_draw* d) {
    if (!d) return;
    memset(d, 0, sizeof *d);
    d->color.r = 1.0f;
    d->color.g = 1.0f;
    d->color.b = 1.0f;
    d->color.a = 1.0f;
}

void zcsr_debug_draw_begin(zcsr_debug_draw* d) {
    if (!d) return;
    d->count = 0u;
    d->overflow = 0u;
}

void zcsr_debug_draw_set_color(zcsr_debug_draw* d, zcsr_debug_color c) {
    if (!d) return;
    d->color = c;
}

bool zcsr_debug_draw_line(zcsr_debug_draw* d, float x0, float y0, float x1, float y1) {
    zcsr_debug_cmd* c = push_cmd(d, ZCSR_DEBUG_CMD_LINE);
    if (!c) return false;
    c->x0 = x0; c->y0 = y0; c->x1 = x1; c->y1 = y1;
    return true;
}

bool zcsr_debug_draw_rect(zcsr_debug_draw* d, float x0, float y0, float x1, float y1) {
    zcsr_debug_cmd* c = push_cmd(d, ZCSR_DEBUG_CMD_RECT);
    if (!c) return false;
    c->x0 = x0; c->y0 = y0; c->x1 = x1; c->y1 = y1;
    return true;
}

bool zcsr_debug_draw_filled_rect(zcsr_debug_draw* d, float x0, float y0, float x1, float y1) {
    zcsr_debug_cmd* c = push_cmd(d, ZCSR_DEBUG_CMD_FILLED_RECT);
    if (!c) return false;
    c->x0 = x0; c->y0 = y0; c->x1 = x1; c->y1 = y1;
    return true;
}

bool zcsr_debug_draw_circle(zcsr_debug_draw* d, float cx, float cy, float radius) {
    zcsr_debug_cmd* c = push_cmd(d, ZCSR_DEBUG_CMD_CIRCLE);
    if (!c) return false;
    c->x0 = cx; c->y0 = cy; c->radius = radius;
    return true;
}

bool zcsr_debug_draw_arrow(zcsr_debug_draw* d, float x0, float y0, float x1, float y1) {
    zcsr_debug_cmd* c = push_cmd(d, ZCSR_DEBUG_CMD_ARROW);
    if (!c) return false;
    c->x0 = x0; c->y0 = y0; c->x1 = x1; c->y1 = y1;
    return true;
}

bool zcsr_debug_draw_text(zcsr_debug_draw* d, float x, float y, const char* text) {
    zcsr_debug_cmd* c = push_cmd(d, ZCSR_DEBUG_CMD_TEXT);
    if (!c) return false;
    c->x0 = x; c->y0 = y;
    c->text = text;
    return true;
}

uint32_t zcsr_debug_draw_grid(zcsr_debug_draw* d, float x0, float y0, float x1, float y1,
                              float cell, uint32_t max_lines) {
    uint32_t added = 0u;
    if (!d || cell <= 0.0f || max_lines == 0u) return 0u;
    if (x1 < x0) { float t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { float t = y0; y0 = y1; y1 = t; }

    for (float x = x0; x <= x1 + 1e-4f && added < max_lines; x += cell) {
        if (zcsr_debug_draw_line(d, x, y0, x, y1)) ++added;
        else break;
    }
    for (float y = y0; y <= y1 + 1e-4f && added < max_lines; y += cell) {
        if (zcsr_debug_draw_line(d, x0, y, x1, y)) ++added;
        else break;
    }
    return added;
}

uint32_t zcsr_debug_draw_flush(const zcsr_debug_draw* d, zcsr_debug_emit_fn cb, void* user) {
    if (!d || !cb) return 0u;
    for (uint32_t i = 0; i < d->count; ++i) cb(&d->cmds[i], user);
    return d->count;
}
