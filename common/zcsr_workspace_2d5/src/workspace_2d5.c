/* integration/testmain.c — minimal 2.5D debug slice (Geometry Proof v1 runtime).
 * World: X east, Y south, Z up, ground z=0. Projection = GAME SHEAR (contract C):
 *   screen_x = (x-cam)*ppu + w/2 ; screen_y = (y-cam)*ppu + h/2 - z*k*ppu
 * Scene: floor, ONE real-height receiving wall (vertical face, real 3D pixels),
 * ONE box footprint 2<=x<=3, -0.5<=y<=0.5, H=2, ONE movable 3D point light
 * (mouse = x/y, W/S = z). Shadows = per-pixel segment-vs-prism (ray_shadows).
 * Debug insets show the G-buffer Position and Normal channels directly.
 * The four Geometry Proof occlusion cases are checked host-side at init. */
#include "zcsr_runtime_shell.h"
#include "zcsr/gl_render.h"
#include "zcsr/input.h"
#include "light_key_contract.h"
#include "zcsr/gl_render_l2d_geo.h"
#include "zcsr/light2d.h"
#include "zcsr/light2d_floor.h"
#include "zcsr/light2d_wall.h"
#include "zcsr/light2d_box.h"
#include "zcsr/light2d_light.h"
#include "zcsr/sprite.h"
#include "zcsr/transform.h"

#include <glad/glad.h>
#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

extern void zcsr_runtime_shell_get_mouse_position(int* x, int* y);

/* ---- world constants (Geometry Proof scene) ---- */
#define BOX_MIN_X 2.0f
#define BOX_MAX_X 3.0f
#define BOX_MIN_Y (-0.5f)
#define BOX_MAX_Y 0.5f
#define BOX_H 2.0f
#define WALL_Y (-2.0f)
#define WALL_H 3.0f
#define FLOOR_MIN_X (-2.0f)
#define FLOOR_MAX_X 10.0f
#define FLOOR_MIN_Y (-2.0f)
#define FLOOR_MAX_Y 2.0f
#define LIGHT_Z_DEFAULT 3.0f
#define LIGHT_RADIUS 14.0f
#define PPU_DEFAULT 20.0f
#define SHEAR_DEFAULT 0.5f

typedef struct {
    zcsr_light2d_ctx* ctx;
    zcsr_camera cam;
    unsigned int dbg_prog;  /* G-buffer channel inspector */
    unsigned int dbg_vao, dbg_vbo;
    int dbg_loc_pos, dbg_loc_nrm, dbg_loc_light, dbg_loc_dst;
    int quality;
    int last_cw, last_ch;
    int frame_index;
    int autoquit;
    int mouse_seen;
    int last_mouse_x, last_mouse_y;
    float light_z;
    int effects_quality;     /* 0 = 关装饰件（rugs），1 = 开 */
    float view_distance;     /* 0..100，越大看得越远（zoom out） */
    zcsr_gl_texture rug_tex; /* programmatic 64x64 fixture, host-owned */
    zcsr_gl_texture nmap_albedo_tex; /* solid albedo for the nmap demo */
    zcsr_gl_texture nmap_tex;        /* 64x64 tangent-space normal map */
    /* 可复用场景组件（引擎层最小单位，demo 只传坐标） */
    zcsr_light2d_floor* floor;
    zcsr_light2d_wall* wall;
    zcsr_light2d_box* box;
    zcsr_light2d_light* mlight; /* movable point light (mouse-driven) */
} light_state;

static unsigned char light2d_buf[64 * 1024];
static light_state g_light;

/* ---- host-side mirror of the shader slab test (proof cross-check) ------- */
static int seg_blocked(float lx, float ly, float lz,
                       float px, float py, float pz)
{
    const float dx = px - lx;
    const float dy = py - ly;
    const float dz = pz - lz;
    float t0 = 1e-4f;
    float t1 = 1.0f - 1e-4f;
    float ta, tb;
    if (dx > -1e-6f && dx < 1e-6f) {
        if (lx < BOX_MIN_X || lx > BOX_MAX_X) return 0;
    } else {
        ta = (BOX_MIN_X - lx) / dx;
        tb = (BOX_MAX_X - lx) / dx;
        if (ta > tb) { float s = ta; ta = tb; tb = s; }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
    }
    if (dy > -1e-6f && dy < 1e-6f) {
        if (ly < BOX_MIN_Y || ly > BOX_MAX_Y) return 0;
    } else {
        ta = (BOX_MIN_Y - ly) / dy;
        tb = (BOX_MAX_Y - ly) / dy;
        if (ta > tb) { float s = ta; ta = tb; tb = s; }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
    }
    if (dz > -1e-6f && dz < 1e-6f) {
        if (lz < 0.0f || lz > BOX_H) return 0;
    } else {
        ta = (0.0f - lz) / dz;
        tb = (BOX_H - lz) / dz;
        if (ta > tb) { float s = ta; ta = tb; tb = s; }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
    }
    return (t0 < t1) ? 1 : 0;
}

static void proof_check(void)
{
    struct { float lz, px; int want; const char* name; } cases[4] = {
        { 3.0f, 7.0f,  1, "L=(0,0,3) P=(7,0,0)  occluded" },
        { 3.0f, 10.0f, 0, "L=(0,0,3) P=(10,0,0) clear" },
        { 5.0f, 4.0f,  1, "L=(0,0,5) P=(4,0,0)  occluded" },
        { 5.0f, 7.0f,  0, "L=(0,0,5) P=(7,0,0)  clear" }
    };
    int i;
    int ok = 1;
    for (i = 0; i < 4; ++i) {
        const int got = seg_blocked(0.0f, 0.0f, cases[i].lz,
                                    cases[i].px, 0.0f, 0.0f);
        printf("[proof] %s: %s\n", cases[i].name,
               got == cases[i].want ? "PASS" : "FAIL");
        if (got != cases[i].want) ok = 0;
    }
    printf("[proof] tip(L_z=3)=x9 tip(L_z=5)=x5 (%s)\n",
           ok ? "all PASS" : "MISMATCH");
}

/* Q4 fixture: 64x64 RGBA, 8px transparent frame, four 24px quadrants
 * (TL red / TR green / BL blue / BR yellow). Image row 0 = v 0 = the
 * texture's top, matching the sprite pipeline's UV convention. */
static void make_rug_fixture(unsigned char* px)
{
    int x;
    int y;
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            unsigned char* p = px + (y * 64 + x) * 4u;
            const int in = (x >= 8 && x < 56 && y >= 8 && y < 56);
            if (!in) {
                p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
            } else if (x < 32 && y < 32) {
                p[0] = 210; p[1] = 70; p[2] = 60; p[3] = 255;   /* TL red */
            } else if (x >= 32 && y < 32) {
                p[0] = 70; p[1] = 190; p[2] = 80; p[3] = 255;   /* TR green */
            } else if (x < 32) {
                p[0] = 60; p[1] = 90; p[2] = 210; p[3] = 255;   /* BL blue */
            } else {
                p[0] = 215; p[1] = 195; p[2] = 70; p[3] = 255;  /* BR yellow */
            }
        }
    }
}

/* Nmap demo albedo: flat neutral gray so the bump reads purely through
 * lighting, no texture detail to confuse the normal-map effect. */
static void make_nmap_albedo(unsigned char* px)
{
    int x;
    int y;
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            unsigned char* p = px + (y * 64 + x) * 4u;
            p[0] = 150; p[1] = 150; p[2] = 150; p[3] = 255;
        }
    }
}

/* Nmap demo: a hemispherical bump. Inside the radius the normal is the
 * sphere normal (dx,dy over r, z = sqrt(1 - that squared)); outside it is
 * the flat up normal (0,0,1). Encoded n*0.5+0.5, stored as plain RGBA
 * (data texture, sampled as-is). Center (31.5,31.5), radius 26. */
static void make_nmap_fixture(unsigned char* px)
{
    const float cx = 31.5f;
    const float cy = 31.5f;
    const float rad = 26.0f;
    int x;
    int y;
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            unsigned char* p = px + (y * 64 + x) * 4u;
            const float dx = ((float)x - cx) / rad;
            const float dy = ((float)y - cy) / rad;
            const float d2 = dx * dx + dy * dy;
            float nx, ny, nz;
            if (d2 >= 1.0f) {
                nx = 0.0f; ny = 0.0f; nz = 1.0f;
            } else {
                nx = dx;
                ny = dy;
                nz = sqrtf(1.0f - d2);
            }
            p[0] = (unsigned char)(nx * 0.5f + 0.5f);
            p[1] = (unsigned char)(ny * 0.5f + 0.5f);
            p[2] = (unsigned char)(nz * 0.5f + 0.5f);
            p[3] = 255;
        }
    }
}

/* G-buffer channel inspector: two inset quads sampling the Position and
 * Normal textures (units 5/6, still bound after the composite). */
static const char* dbg_vs =
    "#version 330 core\n"
    "layout(location=0) in vec2 a_pos;\n"
    "out vec2 v_uv;\n"
    "void main(){\n"
    "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "  v_uv = a_pos * 0.5 + 0.5;\n"
    "}\n";

static const char* dbg_fs =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_pos_tex;  /* unit 5 */\n"
    "uniform sampler2D u_nrm_tex;  /* unit 6 */\n"
    "uniform sampler2D u_light_tex; /* unit 8, HDR accumulation */\n"
    "uniform int u_which;          /* 0 pos, 1 normal, 2 light buffer */\n"
    "out vec4 o_color;\n"
    "void main(){\n"
    "  if (u_which == 0) {\n"
    "    vec3 p = texture(u_pos_tex, v_uv).xyz;\n"
    "    o_color = vec4(clamp(p.x * 0.0625 + 0.5, 0.0, 1.0),\n"
    "                   clamp(p.y * 0.125 + 0.5, 0.0, 1.0),\n"
    "                   clamp(p.z * 0.2, 0.0, 1.0), 1.0);\n"
    "  } else if (u_which == 1) {\n"
    "    vec3 n = texture(u_nrm_tex, v_uv).xyz * 2.0 - 1.0;\n"
    "    o_color = vec4(n * 0.5 + 0.5, 1.0);\n"
    "  } else {\n"
    "    vec3 l = texture(u_light_tex, v_uv).rgb;\n"
    "    o_color = vec4(clamp(l, 0.0, 1.0), 1.0);\n"
    "  }\n"
    "}\n";

static unsigned int compile_shader(unsigned int type, const char* src)
{
    unsigned int shader = glCreateShader(type);
    int ok = 0;
    if (!shader) return 0;
    glShaderSource(shader, 1, &src, 0);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog(shader, (GLsizei)sizeof(log), &len, log);
        if (len < 0) len = 0;
        if (len >= (GLsizei)sizeof(log)) len = (GLsizei)sizeof(log) - 1;
        log[len] = '\0';
        fprintf(stderr, "shader compile failed: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static unsigned int link_program(const char* tag, const char* vs, const char* fs)
{
    unsigned int v = compile_shader(GL_VERTEX_SHADER, vs);
    unsigned int f = compile_shader(GL_FRAGMENT_SHADER, fs);
    unsigned int p;
    int ok = 0;
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        return 0;
    }
    p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(v);
    glDeleteShader(f);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetProgramInfoLog(p, (GLsizei)sizeof(log), &len, log);
        if (len < 0) len = 0;
        if (len >= (GLsizei)sizeof(log)) len = (GLsizei)sizeof(log) - 1;
        log[len] = '\0';
        fprintf(stderr, "%s link failed: %s\n", tag, log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

/* Project one world point through the shared camera's game shear (host
 * mirror for the LOW debug path; the HIGH path projects inside l2d_geo). */
static void project(const light_state* s, int cw, int ch,
                    float wx, float wy, float wz, float* sx, float* sy)
{
    /* 使用 zcsr_camera_world_to_px() 避免重复实现；cw/ch 属于旧接口兼容参数。 */
    (void)cw;
    (void)ch;
    zcsr_camera_world_to_px(&s->cam, wx, wy, wz, sx, sy);
}

/* One world-space quad. HIGH: submitted through the PUBLIC l2d_geo API
 * (engine-owned MRT program; this file defines no G-buffer shader). LOW:
 * flat fill of the projected corner AABB — unlit debug shape only. Faces
 * are submitted ascending in the shear sort key q = k*(y-cy) + z by the
 * caller (floor -> wall -> box sides -> top). */
static void paint_quad(light_state* s, zcsr_gl_renderer* r, int cw, int ch,
                       int low,
                       float ax, float ay, float az,
                       float bx, float by, float bz,
                       float cx, float cy, float cz,
                       float dx, float dy, float dz,
                       float nx, float ny, float nz,
                       float red, float grn, float blu)
{
    const float corners[12] = {
        ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz
    };
    const float albedo[4] = { red, grn, blu, 1.0f };
    const float normal[3] = { nx, ny, nz };
    const float emissive[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (low) {
        float x0, y0, x1, y1, x2, y2, x3, y3;
        project(s, cw, ch, ax, ay, az, &x0, &y0);
        project(s, cw, ch, bx, by, bz, &x1, &y1);
        project(s, cw, ch, cx, cy, cz, &x2, &y2);
        project(s, cw, ch, dx, dy, dz, &x3, &y3);
        {
            float min_x = x0, max_x = x0, min_y = y0, max_y = y0;
            float xs[4]; float ys[4];
            int i;
            xs[0] = x0; xs[1] = x1; xs[2] = x2; xs[3] = x3;
            ys[0] = y0; ys[1] = y1; ys[2] = y2; ys[3] = y3;
            for (i = 1; i < 4; ++i) {
                if (xs[i] < min_x) min_x = xs[i];
                if (xs[i] > max_x) max_x = xs[i];
                if (ys[i] < min_y) min_y = ys[i];
                if (ys[i] > max_y) max_y = ys[i];
            }
            zcsr_gl_fill_rect(r, min_x, min_y, max_x - min_x, max_y - min_y,
                              red, grn, blu, 1.0f);
        }
        return;
    }
    (void)zcsr_gl_l2d_geo_face(r, corners, albedo, normal, emissive);
}

static void draw_scene(light_state* s, zcsr_gl_renderer* r, int cw, int ch,
                       int low, float lx, float ly, float lz)
{
    /* 修复：不用 zcsr_gl_clear（它会强制切回 back buffer），改成在当前渲染目标上画全屏矩形 */
    if (low) {
        zcsr_gl_sprite bg = {0};
        bg.r = 0.02f; bg.g = 0.02f; bg.b = 0.02f; bg.a = 1.0f;
        bg.dst = (zcsr_rectf){0, 0, (float)cw, (float)ch};
        zcsr_gl_submit(r, &bg);
        zcsr_gl_flush(r);
    } else {
        /* one begin/end pair around all faces; ascending shear-key order */
        if (!zcsr_gl_l2d_geo_begin(r, &s->cam)) {
            fprintf(stderr, "FAIL: l2d_geo_begin\n");
            return;
        }
    }

    /* floor / wall / box：HIGH 路径走引擎组件 API（只传坐标），LOW debug 路径保留
     * 平面矩形轮廓（debug 形状不属于通用组件）。 */
    if (!low) {
        zcsr_light2d_floor_render(s->floor, r);
        zcsr_light2d_wall_render(s->wall, r);
        zcsr_light2d_box_render(s->box, r);
    } else {
        paint_quad(s, r, cw, ch, low,
                   FLOOR_MIN_X, FLOOR_MIN_Y, 0.0f,
                   FLOOR_MAX_X, FLOOR_MIN_Y, 0.0f,
                   FLOOR_MAX_X, FLOOR_MAX_Y, 0.0f,
                   FLOOR_MIN_X, FLOOR_MAX_Y, 0.0f,
                   0.0f, 0.0f, 1.0f,
                   0.45f, 0.47f, 0.52f);
        paint_quad(s, r, cw, ch, low,
                   FLOOR_MIN_X, WALL_Y, 0.0f,
                   FLOOR_MAX_X, WALL_Y, 0.0f,
                   FLOOR_MAX_X, WALL_Y, WALL_H,
                   FLOOR_MIN_X, WALL_Y, WALL_H,
                   0.0f, 1.0f, 0.0f,
                   0.38f, 0.36f, 0.34f);
        paint_quad(s, r, cw, ch, low,
                   BOX_MIN_X, BOX_MIN_Y, 0.0f, BOX_MAX_X, BOX_MIN_Y, 0.0f,
                   BOX_MAX_X, BOX_MIN_Y, BOX_H, BOX_MIN_X, BOX_MIN_Y, BOX_H,
                   0.0f, -1.0f, 0.0f, 0.30f, 0.30f, 0.34f);
        paint_quad(s, r, cw, ch, low,
                   BOX_MIN_X, BOX_MIN_Y, 0.0f, BOX_MIN_X, BOX_MAX_Y, 0.0f,
                   BOX_MIN_X, BOX_MAX_Y, BOX_H, BOX_MIN_X, BOX_MIN_Y, BOX_H,
                   -1.0f, 0.0f, 0.0f, 0.38f, 0.38f, 0.42f);
        paint_quad(s, r, cw, ch, low,
                   BOX_MAX_X, BOX_MIN_Y, 0.0f, BOX_MAX_X, BOX_MAX_Y, 0.0f,
                   BOX_MAX_X, BOX_MAX_Y, BOX_H, BOX_MAX_X, BOX_MIN_Y, BOX_H,
                   1.0f, 0.0f, 0.0f, 0.38f, 0.38f, 0.42f);
        paint_quad(s, r, cw, ch, low,
                   BOX_MIN_X, BOX_MAX_Y, 0.0f, BOX_MAX_X, BOX_MAX_Y, 0.0f,
                   BOX_MAX_X, BOX_MAX_Y, BOX_H, BOX_MIN_X, BOX_MAX_Y, BOX_H,
                   0.0f, 1.0f, 0.0f, 0.52f, 0.52f, 0.56f);
        paint_quad(s, r, cw, ch, low,
                   BOX_MIN_X, BOX_MIN_Y, BOX_H, BOX_MAX_X, BOX_MIN_Y, BOX_H,
                   BOX_MAX_X, BOX_MAX_Y, BOX_H, BOX_MIN_X, BOX_MAX_Y, BOX_H,
                   0.0f, 0.0f, 1.0f, 0.80f, 0.82f, 0.86f);
    }

    /* Q4 textured rugs (HIGH only — the LOW debug path stays flat):
     * rug A maps the FULL fixture (four quadrants visible), rug B maps the
     * sub-rect UV (0.5,0.5)-(1,1) so its whole surface shows the fixture's
     * BR quadrant — same world size, different texture region proves the UV
     * path. The 8px transparent frame must DISCARD, leaving the floor's
     * earlier G-buffer write intact beneath both rugs. */
    if (!low && s->effects_quality && s->rug_tex != 0u) {
        const float tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        const float emiss[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        const float up[3] = { 0.0f, 0.0f, 1.0f };
        {
            const float ca[12] = { -1.6f, 0.4f, 0.0f, -0.6f, 0.4f, 0.0f,
                                   -0.6f, 1.4f, 0.0f, -1.6f, 1.4f, 0.0f };
            const float ua[8] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
            (void)zcsr_gl_l2d_geo_tex_face(r, s->rug_tex, ca, ua, tint, up, emiss);
        }
        {
            const float cb[12] = { 0.6f, 0.5f, 0.0f, 1.6f, 0.5f, 0.0f,
                                   1.6f, 1.5f, 0.0f, 0.6f, 1.5f, 0.0f };
            const float ub[8] = { 0.5f, 0.5f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f };
            (void)zcsr_gl_l2d_geo_tex_face(r, s->rug_tex, cb, ub, tint, up, emiss);
        }

        /* Adapter smoke test: zcsr_sprite_draw_lit builds the same corner/uv
         * geometry from a transform + asset + ground_z and submits it via the
         * public API. Placed at (2.5, 1.8) on the ground, 1x1 world unit,
         * whole-texture, unit tint. If this renders identically to a hand-built
         * quad, the §14.4 contract holds. */
        {
            zcsr_sprite_asset asset;
            zcsr_sprite_instance inst;
            zcsr_transform tr;
            asset.tex = s->rug_tex;
            asset.u0 = 0.0f; asset.v0 = 0.0f;
            asset.u1 = 1.0f; asset.v1 = 1.0f;
            asset.w = 1.0f; asset.h = 1.0f;
            inst.asset = &asset;
            inst.r = 1.0f; inst.g = 1.0f; inst.b = 1.0f; inst.a = 1.0f;
            tr.x = 2.5f; tr.y = 1.8f;
            tr.rotation = 0.0f;
            tr.scale_x = 1.0f; tr.scale_y = 1.0f;
            zcsr_sprite_draw_lit(r, &tr, &inst, 0.0f);
        }
    }

    /* NORMAL-MAP demo (HIGH only): a ground quad whose G2 normal comes from
     * the tangent-space normal texture via TBN. Up normal (0,0,1), tangent
     * (1,0,0) aligned with +U; the hemisphere bump shows as curved shading
     * under the moving point light. Sits in free space x[4.5,6] y[.7,1.7]. */
    if (!low && s->nmap_tex != 0u && s->nmap_albedo_tex != 0u) {
        const float corners[12] = {
            4.5f, 0.7f, 0.0f,  6.0f, 0.7f, 0.0f,
            6.0f, 1.7f, 0.0f,  4.5f, 1.7f, 0.0f
        };
        const float uv[8] = { 0.0f, 0.0f, 1.0f, 0.0f,
                              1.0f, 1.0f, 0.0f, 1.0f };
        const float tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        const float up[3] = { 0.0f, 0.0f, 1.0f };
        const float tangent[3] = { 1.0f, 0.0f, 0.0f };
        const float emiss[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (!zcsr_gl_l2d_geo_nmap_face(r, s->nmap_albedo_tex, s->nmap_tex,
                                        corners, uv, tint, up, tangent, emiss)) {
            fprintf(stderr, "WARN: nmap face submission failed\n");
        }
    }

    /* flame marker: small quad AT the light's 3D position (shear shows z) */
    paint_quad(s, r, cw, ch, low,
               lx - 0.08f, ly - 0.08f, lz, lx + 0.08f, ly - 0.08f, lz,
               lx + 0.08f, ly + 0.08f, lz, lx - 0.08f, ly + 0.08f, lz,
               0.0f, 0.0f, 1.0f,
               0.9f, 0.7f, 0.3f);
    if (low) {
        zcsr_gl_flush(r);
    } else if (!zcsr_gl_l2d_geo_end(r)) {
        fprintf(stderr, "FAIL: l2d_geo_end\n");
    }
}

/* ---- module callbacks (window.h contract) ---------------------------------- */
static bool light_module_init(zcsr_gl_renderer* r, int cw, int ch, void* user)
{
    light_state* s = (light_state*)user;
    zcsr_light2d_options opt;
    zcsr_light2d_occluder occ;
    const float footprint[8] = {
        BOX_MIN_X, BOX_MIN_Y, BOX_MAX_X, BOX_MIN_Y,
        BOX_MAX_X, BOX_MAX_Y, BOX_MIN_X, BOX_MAX_Y
    };

    proof_check();

    s->ctx = zcsr_light2d_create(r,
                                 ZCSR_LIGHT_QUALITY_HIGH,
                                 ZCSR_LIGHT_QUALITY_HIGH,
                                 cw, ch, light2d_buf, sizeof(light2d_buf));
    if (!s->ctx) return false;

    s->dbg_prog = link_program("channel inspector", dbg_vs, dbg_fs);
    if (!s->dbg_prog) return false;
    s->dbg_loc_pos = glGetUniformLocation(s->dbg_prog, "u_pos_tex");
    s->dbg_loc_nrm = glGetUniformLocation(s->dbg_prog, "u_nrm_tex");
    s->dbg_loc_light = glGetUniformLocation(s->dbg_prog, "u_light_tex");
    s->dbg_loc_dst = glGetUniformLocation(s->dbg_prog, "u_which");
    glGenVertexArrays(1, &s->dbg_vao);
    glGenBuffers(1, &s->dbg_vbo);
    glBindVertexArray(s->dbg_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s->dbg_vbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    /* 根据视口大小计算合适的 PPU，让场景等比例填满视口（Contain Fit）
     * 场景大小：宽度 12 单位（x 从 -2 到 10），深度 4 单位（y 从 -2 到 2）
     */
    float scene_w = 12.0f;
    float scene_h = 4.0f;
    float ppu_x = (float)cw / scene_w;
    float ppu_y = (float)ch / scene_h;
    float ppu = (ppu_x < ppu_y) ? ppu_x : ppu_y;

    zcsr_camera_init(&s->cam, cw, ch, 4.0f, 0.0f, ppu, SHEAR_DEFAULT);
    /* 视角高度可调：SHEAR_DEFAULT=0.5 等于俯仰 63.4 度（偏高的调试视角）。
     * 真实游戏常要压低到 50-55 度，ZCSR_VIEW_PITCH 直接给角度。 */
    {
        const char* pv = getenv("ZCSR_VIEW_PITCH");
        if (pv && pv[0]) {
            char* end;
            double v = strtod(pv, &end);
            if (end && *end == '\0' && v > 0.0 && v < 90.0) {
                zcsr_camera_set_pitch_deg(&s->cam, (float)v);
            }
        }
    }
    zcsr_light2d_set_camera(s->ctx, &s->cam);

    opt.shadows = true;
    opt.normals = true;
    opt.gain = 3.0f;
    opt.light_scale_pct = 50;
    opt.bloom = false;
    opt.ray_shadows = true; /* Geometry Proof semantics */
    if (!zcsr_light2d_set_options(s->ctx, &opt)) {
        fprintf(stderr, "FAIL: options rejected\n");
        return false;
    }

    occ.xy = footprint;
    occ.count = 4u;
    occ.height = BOX_H;
    occ.factor = 1.0f;
    occ.distance = 0.0f;
    occ.reverse = false;
    occ.self_shadow = false;
    occ.auto_disable = false;
    zcsr_light2d_clear_occluders(s->ctx);
    if (!zcsr_light2d_add_occluder(s->ctx, &occ)) {
        fprintf(stderr, "FAIL: occluder rejected\n");
        return false;
    }

    s->quality = (int)ZCSR_LIGHT_QUALITY_HIGH;
    s->last_cw = cw;
    s->last_ch = ch;
    s->frame_index = 0;
    s->mouse_seen = 0;
    s->last_mouse_x = 0;
    s->last_mouse_y = 0;
    s->light_z = LIGHT_Z_DEFAULT;

    /* Q4 fixture texture (host-owned: created here, destroyed in shutdown) */
    {
        static unsigned char rug_px[64 * 64 * 4];
        make_rug_fixture(rug_px);
        s->rug_tex = zcsr_gl_texture_create(r, rug_px, 64, 64);
        if (s->rug_tex == 0u) {
            fprintf(stderr, "WARN: rug fixture texture creation failed\n");
        }
    }

    /* Nmap demo textures (host-owned: created here, destroyed in shutdown). */
    {
        static unsigned char albedo_px[64 * 64 * 4];
        static unsigned char nmap_px[64 * 64 * 4];
        make_nmap_albedo(albedo_px);
        make_nmap_fixture(nmap_px);
        s->nmap_albedo_tex = zcsr_gl_texture_create(r, albedo_px, 64, 64);
        s->nmap_tex = zcsr_gl_texture_create(r, nmap_px, 64, 64);
        if (s->nmap_albedo_tex == 0u || s->nmap_tex == 0u) {
            fprintf(stderr, "WARN: nmap fixture texture creation failed\n");
        }
    }

    /* 可复用场景组件：demo 只传坐标，几何提交由引擎模块完成 */
    zcsr_light2d_floor_create(&s->floor);
    zcsr_light2d_floor_set_size(s->floor, FLOOR_MIN_X, FLOOR_MAX_X, FLOOR_MIN_Y, FLOOR_MAX_Y);
    zcsr_light2d_floor_set_tint(s->floor, 0.45f, 0.47f, 0.52f, 1.0f);

    zcsr_light2d_wall_create(&s->wall);
    zcsr_light2d_wall_set_geometry(s->wall, WALL_Y, WALL_H, FLOOR_MIN_X, FLOOR_MAX_X);
    zcsr_light2d_wall_set_tint(s->wall, 0.38f, 0.36f, 0.34f, 1.0f);

    zcsr_light2d_box_create(&s->box);
    zcsr_light2d_box_set_geometry(s->box, BOX_MIN_X, BOX_MAX_X, BOX_MIN_Y, BOX_MAX_Y, BOX_H);
    zcsr_light2d_box_set_tint(s->box, 0.5f, 0.5f, 0.55f, 1.0f);

    zcsr_light2d_light_create(&s->mlight);
    zcsr_light2d_light_set_radius(s->mlight, LIGHT_RADIUS);
    zcsr_light2d_light_set_color(s->mlight, 1.0f, 0.85f, 0.6f);
    zcsr_light2d_light_set_intensity(s->mlight, 1.0f);

    printf("[light] 2.5D slice init %dx%d (floor+wall+H2 box+3D light+rugs)\n", cw, ch);
    return true;
}

static void light_module_shutdown(zcsr_gl_renderer* r, void* user)
{
    light_state* s = (light_state*)user;
    (void)r;
    if (s->dbg_prog) glDeleteProgram(s->dbg_prog);
    if (s->dbg_vbo) glDeleteBuffers(1, &s->dbg_vbo);
    if (s->dbg_vao) glDeleteVertexArrays(1, &s->dbg_vao);
    if (s->rug_tex != 0u) zcsr_gl_texture_destroy(r, s->rug_tex);
    if (s->nmap_albedo_tex != 0u) zcsr_gl_texture_destroy(r, s->nmap_albedo_tex);
    if (s->nmap_tex != 0u) zcsr_gl_texture_destroy(r, s->nmap_tex);
    zcsr_light2d_light_destroy(s->mlight); s->mlight = NULL;
    zcsr_light2d_box_destroy(s->box);     s->box = NULL;
    zcsr_light2d_wall_destroy(s->wall);   s->wall = NULL;
    zcsr_light2d_floor_destroy(s->floor); s->floor = NULL;
    if (s->ctx) zcsr_light2d_destroy(s->ctx);
    s->ctx = NULL;
    printf("[light] module shutdown\n");
}


static void light_module_frame(zcsr_gl_renderer* r, int cw, int ch, void* user)
{
    light_state* s = (light_state*)user;
    zcsr_light light;
    float lx = 0.0f;
    float ly = 0.0f;
    float lz;
    int raw_mouse_x = 0;
    int raw_mouse_y = 0;
    const int frame_index = s->frame_index;

    if (s->ctx == NULL) return;
    lz = s->light_z;

    if (cw != s->last_cw || ch != s->last_ch) {
        zcsr_camera_set_viewport(&s->cam, cw, ch);
        if (zcsr_light2d_resize(s->ctx, cw, ch)) {
            s->last_cw = cw;
            s->last_ch = ch;
        }
    }

    zcsr_runtime_shell_get_mouse_position(&raw_mouse_x, &raw_mouse_y);
    if (!s->autoquit) {
        if (frame_index == 0) {
            s->last_mouse_x = raw_mouse_x;
            s->last_mouse_y = raw_mouse_y;
        } else if (raw_mouse_x != s->last_mouse_x ||
                   raw_mouse_y != s->last_mouse_y) {
            s->mouse_seen = 1;
        }
        s->last_mouse_x = raw_mouse_x;
        s->last_mouse_y = raw_mouse_y;
        if (s->mouse_seen) {
            zcsr_camera_px_to_world(&s->cam, (float)raw_mouse_x, (float)raw_mouse_y,
                                    &lx, &ly);
        }
    }

    if (s->autoquit) {
        if (frame_index == 30) {
            printf("[light] autoquit frame 30: L=(%.2f,%.2f,%.2f)\n", lx, ly, lz);
        }
    }

    /* 可移动自由灯：组件只存参数，to_zcsr_light() 填 zcsr_light 结构 */
    zcsr_light2d_light_set_position(s->mlight, lx, ly, lz);
    zcsr_light2d_light_to_zcsr_light(s->mlight, &light);

    if (!zcsr_light2d_begin_geometry(s->ctx)) {
        fprintf(stderr, "FAIL: begin_geometry\n");
        return;
    }
    draw_scene(s, r, cw, ch,
               s->quality == (int)ZCSR_LIGHT_QUALITY_LOW ? 1 : 0,
               lx, ly, lz);

    if (!zcsr_light2d_begin_lighting(s->ctx) ||
        !zcsr_light2d_add_light(s->ctx, &light)) {
        fprintf(stderr, "FAIL: lighting pass\n");
        return;
    }

    /* Dedicated light over the nmap bump (HIGH): a short-radius white point
     * light directly above the hemisphere so its curved shading reads
     * clearly. Radius 3.0 keeps it from reaching the gpu_shadow probes. */
    if (s->quality == (int)ZCSR_LIGHT_QUALITY_HIGH) {
        zcsr_light nlight;
        memset(&nlight, 0, sizeof(nlight));
        nlight.x = 5.25f;
        nlight.y = 1.2f;
        nlight.height = 2.5f;
        nlight.radius = 3.0f;
        nlight.decay = 0.5f;
        nlight.type = ZCSR_LIGHT_POINT;
        nlight.r = 1.0f;
        nlight.g = 1.0f;
        nlight.b = 1.0f;
        nlight.a = 1.4f;
        if (!zcsr_light2d_add_light(s->ctx, &nlight)) {
            fprintf(stderr, "WARN: nmap light rejected, skipping\n");
        }
    }

    /* SPOT demo (HIGH only): a bright cyan cone aimed straight down onto the
     * open southern floor, showing a bright inner core and a soft penumbra
     * ring. The footprint stays clear of the two gpu_shadow probes.
     * Point falloff still applies; the cone gates it. */
    if (s->quality == (int)ZCSR_LIGHT_QUALITY_HIGH) {
        zcsr_light spot;
        memset(&spot, 0, sizeof(spot));
        spot.x = 0.0f;
        spot.y = -1.0f;
        spot.height = 3.0f;
        spot.radius = 6.0f;
        spot.decay = 0.5f;
        spot.dir_x = 0.0f;
        spot.dir_y = 0.0f;
        spot.dir_z = -1.0f;
        spot.inner_angle = 0.20f;
        spot.outer_angle = 0.45f;
        spot.type = ZCSR_LIGHT_SPOT;
        spot.r = 0.35f;
        spot.g = 0.8f;
        spot.b = 1.0f;
        spot.a = 2.5f;
        if (!zcsr_light2d_add_light(s->ctx, &spot)) {
            /* non-fatal: skip the demo rather than leave the lighting phase stuck */
            fprintf(stderr, "WARN: spot light rejected, skipping\n");
        }
    }

    if (!zcsr_light2d_end_lighting(s->ctx)) {
        fprintf(stderr, "FAIL: end lighting\n");
        return;
    }


    /* debug 信息输出到终端，不渲染到画面 */
    if ((s->frame_index % 60) == 0) {
        printf("[light] frame=%d L=(%.2f,%.2f,%.2f) tip=%.1f\n",
               s->frame_index, lx, ly, lz, 3.0f * BOX_MAX_X / lz);
    }

    ++s->frame_index;
}

static bool light_module_key(int key, void* user)
{
    light_state* s = (light_state*)user;
    zcsr_light_key_action action;
    if (s->ctx == NULL) return false;
    if (s->autoquit) return false; /* autoquit: no interactive key handling */
    action = zcsr_light_key_classify(key);
    switch (action) {
    case ZCSR_LIGHT_KEY_QUALITY: {
        int next_quality = s->quality;
        zcsr_light_key_apply(action, &next_quality, &s->light_z);
        if (zcsr_light2d_set_quality(s->ctx, (zcsr_light_quality)next_quality)) {
            s->quality = next_quality;
            printf("[light] quality -> %s\n",
                   s->quality == (int)ZCSR_LIGHT_QUALITY_LOW ? "LOW" : "HIGH");
        }
        return true;
    }
    case ZCSR_LIGHT_KEY_Z_UP:
    case ZCSR_LIGHT_KEY_Z_DOWN:
        zcsr_light_key_apply(action, &s->quality, &s->light_z);
        printf("[light] light z -> %.2f (tip x=%.1f)\n", s->light_z,
               3.0f * BOX_MAX_X / s->light_z);
        return true;
    default:
        return false;
    }
}

/* 导出的 light2d 演示模块 */
const zcsr_runtime_shell_module light2d_module = {
    "LIGHT 2.5D",
    &g_light,
    light_module_init,
    light_module_shutdown,
    light_module_frame,
    light_module_key,
};

/* light2d 模块初始化函数 */
/* Effects Quality / View Distance 的真落点：
 * effects=0 时跳过 Q4 纹理地毯（draw_scene 里唯一可省的装饰件）；
 * view_distance 换算成相机 ppu：0..100 -> ppu 30..15，越大视野越远。 */
bool zcsr_light2d_module_set_scene(int effects, float view_distance) {
    if (view_distance < 0.0f) view_distance = 0.0f;
    if (view_distance > 100.0f) view_distance = 100.0f;
    g_light.effects_quality = effects != 0;
    g_light.view_distance = view_distance;
    zcsr_camera_set_zoom(&g_light.cam, 30.0f - 0.15f * view_distance);
    return true;
}

/* ---- 设置页的模块级落点 ------------------------------------------------
 * 设置页只存值；Graphics 的阴影/光照分辨率/后处理、Default FOV 要落在这里，
 * 否则就是"有 UI、有存盘、画面不变"。 */
bool zcsr_light2d_module_set_render_options(int shadows, int light_scale_pct,
                                            int bloom, int ray_shadows) {
    if (!g_light.ctx) return false;
    zcsr_light2d_options opt;
    zcsr_light2d_get_options(g_light.ctx, &opt);
    if (light_scale_pct != 25 && light_scale_pct != 50 && light_scale_pct != 100)
        light_scale_pct = 50;
    opt.shadows = shadows != 0;
    opt.light_scale_pct = light_scale_pct;
    opt.bloom = bloom != 0;
    opt.ray_shadows = ray_shadows != 0;
    return zcsr_light2d_set_options(g_light.ctx, &opt);
}

/* FOV 在 2.5D 正交切片里等价于俯仰角：FOV 大 -> 看得平 -> pitch 小。
 * 90° 对应现有 SHEAR_DEFAULT（pitch≈63.4°），按 0.3°/度线性拉开。 */
bool zcsr_light2d_module_set_fov_deg(float fov_deg) {
    if (fov_deg < 30.0f) fov_deg = 30.0f;
    if (fov_deg > 150.0f) fov_deg = 150.0f;
    zcsr_camera_set_pitch_deg(&g_light.cam, 63.4f + (90.0f - fov_deg) * 0.3f);
    return true;
}

float zcsr_light2d_module_get_pitch_deg(void) {
    return zcsr_camera_get_pitch_deg(&g_light.cam);
}

void light2d_module_init(void)
{
    memset(&g_light, 0, sizeof(g_light));
    g_light.quality = (int)ZCSR_LIGHT_QUALITY_HIGH;
    g_light.light_z = LIGHT_Z_DEFAULT;
    g_light.effects_quality = 1;
    g_light.view_distance = 100.0f;
    zcsr_camera_init(&g_light.cam, 1280, 800, 4.0f, 0.0f, PPU_DEFAULT, SHEAR_DEFAULT);
    {
        const char* pv = getenv("ZCSR_VIEW_PITCH");
        if (pv && pv[0]) {
            char* end;
            double v = strtod(pv, &end);
            if (end && *end == '\0' && v > 0.0 && v < 90.0) {
                zcsr_camera_set_pitch_deg(&g_light.cam, (float)v);
            }
        }
    }
}
