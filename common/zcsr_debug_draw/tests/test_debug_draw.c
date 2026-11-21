#include "zcsr_debug_draw.h"

#include <stdio.h>
#include <string.h>

static int expect(int cond, const char* label) {
    if (!cond) printf("FAIL: %s\n", label);
    return cond ? 0 : 1;
}

static uint32_t g_emitted;
static void count_emit(const zcsr_debug_cmd* cmd, void* user) {
    (void)cmd;
    (void)user;
    ++g_emitted;
}

int main(void) {
    static zcsr_debug_draw d;
    zcsr_debug_color red = { 1.0f, 0.0f, 0.0f, 1.0f };
    int fails = 0;

    zcsr_debug_draw_init(&d);
    zcsr_debug_draw_begin(&d);
    zcsr_debug_draw_set_color(&d, red);
    zcsr_debug_draw_line(&d, 0, 0, 10, 10);
    zcsr_debug_draw_rect(&d, 2, 3, 8, 9);
    zcsr_debug_draw_filled_rect(&d, 4, 5, 6, 7);
    zcsr_debug_draw_circle(&d, 12, 12, 4);
    zcsr_debug_draw_arrow(&d, 20, 20, 30, 30);
    zcsr_debug_draw_text(&d, 40, 40, "hello");

    fails += expect(d.count == 6, "six primitives");
    fails += expect(d.cmds[0].type == ZCSR_DEBUG_CMD_LINE, "line type");
    fails += expect(d.cmds[1].type == ZCSR_DEBUG_CMD_RECT, "rect type");
    fails += expect(d.cmds[2].type == ZCSR_DEBUG_CMD_FILLED_RECT, "filled rect type");
    fails += expect(d.cmds[3].type == ZCSR_DEBUG_CMD_CIRCLE, "circle type");
    fails += expect(d.cmds[4].type == ZCSR_DEBUG_CMD_ARROW, "arrow type");
    fails += expect(d.cmds[5].type == ZCSR_DEBUG_CMD_TEXT, "text type");
    fails += expect(d.cmds[5].text != NULL && strcmp(d.cmds[5].text, "hello") == 0,
                    "text pointer kept");

    zcsr_debug_draw_begin(&d);
    {
        uint32_t added = zcsr_debug_draw_grid(&d, 0.0f, 0.0f, 10.0f, 10.0f, 5.0f, 100);
        fails += expect(added == 6, "grid lines 6");
        fails += expect(d.count == 6, "grid commands 6");
    }

    g_emitted = 0;
    {
        uint32_t emitted = zcsr_debug_draw_flush(&d, count_emit, NULL);
        fails += expect(emitted == 6 && g_emitted == 6, "flush emits all");
    }

    zcsr_debug_draw_begin(&d);
    for (uint32_t i = 0; i < ZCSR_DEBUG_DRAW_MAX_COMMANDS; ++i)
        zcsr_debug_draw_line(&d, 0, 0, 1, 1);
    fails += expect(d.count == ZCSR_DEBUG_DRAW_MAX_COMMANDS, "capacity filled");
    fails += expect(zcsr_debug_draw_line(&d, 0, 0, 1, 1) == false, "overflow rejected");
    fails += expect(d.count == ZCSR_DEBUG_DRAW_MAX_COMMANDS, "overflow count stable");
    fails += expect(d.overflow == 1, "overflow flag set");

    if (!fails) printf("test_debug_draw: PASS\n");
    return fails ? 1 : 0;
}
