/**
 * PSVitaman - User Interface & Cassette Rendering Engine Implementation
 */

#include "ui.h"
#include "utils.h"
#include "qrcodegen.h"
#include "http_server.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(__psp2__) || defined(__VITA__)
#include <vita2d.h>
#else
/* Fallback mocks for non-Vita compilers */
typedef void* vita2d_pgf;
#define RGBA8(r,g,b,a) ((((a)&0xFF)<<24)|(((b)&0xFF)<<16)|(((g)&0xFF)<<8)|((r)&0xFF))
static void vita2d_start_drawing(void) {}
static void vita2d_clear_screen(void) {}
static void vita2d_draw_rectangle(float x, float y, float w, float h, unsigned int c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
static void vita2d_draw_fill_circle(float x, float y, float r, unsigned int c) { (void)x;(void)y;(void)r;(void)c; }
static void vita2d_draw_line(float x0, float y0, float x1, float y1, unsigned int c) { (void)x0;(void)y0;(void)x1;(void)y1;(void)c; }
static void vita2d_set_clip_rectangle(int x, int y, int w, int h) { (void)x;(void)y;(void)w;(void)h; }
static void vita2d_disable_clipping(void) {}
static void vita2d_pgf_draw_text(vita2d_pgf *f, int x, int y, unsigned int c, float s, const char *t) { (void)f;(void)x;(void)y;(void)c;(void)s;(void)t; }
static int vita2d_pgf_text_width(vita2d_pgf *f, float s, const char *t) { (void)f;(void)s; return (int)(strlen(t) * 10); }
static int vita2d_pgf_text_height(vita2d_pgf *f, float s, const char *t) { (void)f;(void)s;(void)t; return 16; }
static void vita2d_end_drawing(void) {}
static void vita2d_swap_buffers(void) {}
static vita2d_pgf *vita2d_load_default_pgf(void) { return (void*)1; }
static void vita2d_free_pgf(vita2d_pgf *f) { (void)f; }
#endif

/* Theme Color Palette Definition */
typedef struct {
    const char *name;
    const char *model_stamp;
    unsigned int bg_chassis;
    unsigned int border_light;
    unsigned int border_dark;
    unsigned int cassette_shell;
    unsigned int cassette_inner;
    unsigned int label_bg;
    unsigned int label_stripe_a;
    unsigned int label_stripe_b;
    unsigned int label_text;
    unsigned int btn_normal;
    unsigned int btn_pressed;
    unsigned int btn_border_hi;
    unsigned int btn_border_lo;
    unsigned int btn_active_led;
    unsigned int btn_theme_special;
    unsigned int btn_theme_special_dark;
    unsigned int text_primary;
    unsigned int text_muted;
    unsigned int text_accent;
    unsigned int spool_hub;
    unsigned int spool_gear;
    unsigned int tape_brown;
} AppTheme;

static const AppTheme s_themes[THEME_COUNT] = {
    [THEME_TPS_L2] = {
        .name = "1979 TPS-L2 Blue",
        .model_stamp = "SONY WALKMAN - TPS-L2 STEREO",
        .bg_chassis = RGBA8(28, 48, 80, 255),
        .border_light = RGBA8(165, 175, 195, 255),
        .border_dark = RGBA8(12, 18, 30, 255),
        .cassette_shell = RGBA8(28, 32, 42, 255),
        .cassette_inner = RGBA8(16, 18, 24, 255),
        .label_bg = RGBA8(242, 239, 230, 255),
        .label_stripe_a = RGBA8(225, 45, 35, 255),
        .label_stripe_b = RGBA8(35, 105, 215, 255),
        .label_text = RGBA8(28, 30, 36, 255),
        .btn_normal = RGBA8(44, 52, 68, 255),
        .btn_pressed = RGBA8(24, 28, 38, 255),
        .btn_border_hi = RGBA8(85, 98, 122, 255),
        .btn_border_lo = RGBA8(14, 18, 26, 255),
        .btn_active_led = RGBA8(30, 215, 96, 255),
        .btn_theme_special = RGBA8(225, 55, 38, 255),
        .btn_theme_special_dark = RGBA8(145, 28, 18, 255),
        .text_primary = RGBA8(245, 248, 252, 255),
        .text_muted = RGBA8(150, 162, 180, 255),
        .text_accent = RGBA8(30, 215, 96, 255),
        .spool_hub = RGBA8(245, 245, 245, 255),
        .spool_gear = RGBA8(160, 165, 175, 255),
        .tape_brown = RGBA8(60, 42, 28, 255),
    },
    [THEME_SPORTS_YELLOW] = {
        .name = "1983 WM-F5 Sports",
        .model_stamp = "SONY WALKMAN - SPORTS WM-F5",
        .bg_chassis = RGBA8(228, 182, 18, 255),
        .border_light = RGBA8(255, 220, 65, 255),
        .border_dark = RGBA8(115, 90, 8, 255),
        .cassette_shell = RGBA8(24, 25, 28, 255),
        .cassette_inner = RGBA8(14, 15, 18, 255),
        .label_bg = RGBA8(235, 235, 235, 255),
        .label_stripe_a = RGBA8(228, 182, 18, 255),
        .label_stripe_b = RGBA8(20, 20, 24, 255),
        .label_text = RGBA8(20, 22, 26, 255),
        .btn_normal = RGBA8(32, 34, 38, 255),
        .btn_pressed = RGBA8(16, 18, 20, 255),
        .btn_border_hi = RGBA8(65, 70, 78, 255),
        .btn_border_lo = RGBA8(10, 12, 14, 255),
        .btn_active_led = RGBA8(255, 190, 0, 255),
        .btn_theme_special = RGBA8(0, 165, 175, 255),
        .btn_theme_special_dark = RGBA8(0, 105, 115, 255),
        .text_primary = RGBA8(20, 22, 26, 255),
        .text_muted = RGBA8(70, 75, 85, 255),
        .text_accent = RGBA8(0, 140, 150, 255),
        .spool_hub = RGBA8(240, 240, 240, 255),
        .spool_gear = RGBA8(120, 125, 135, 255),
        .tape_brown = RGBA8(60, 42, 28, 255),
    },
    [THEME_GRAPHITE_DD] = {
        .name = "1982 WM-DD Graphite",
        .model_stamp = "SONY WALKMAN - PROFESSIONAL DD",
        .bg_chassis = RGBA8(30, 32, 36, 255),
        .border_light = RGBA8(195, 200, 210, 255),
        .border_dark = RGBA8(12, 14, 16, 255),
        .cassette_shell = RGBA8(22, 24, 28, 255),
        .cassette_inner = RGBA8(14, 15, 18, 255),
        .label_bg = RGBA8(225, 220, 205, 255),
        .label_stripe_a = RGBA8(190, 155, 60, 255),
        .label_stripe_b = RGBA8(180, 35, 45, 255),
        .label_text = RGBA8(24, 25, 28, 255),
        .btn_normal = RGBA8(42, 44, 50, 255),
        .btn_pressed = RGBA8(20, 22, 25, 255),
        .btn_border_hi = RGBA8(95, 102, 115, 255),
        .btn_border_lo = RGBA8(12, 14, 16, 255),
        .btn_active_led = RGBA8(235, 45, 45, 255),
        .btn_theme_special = RGBA8(185, 32, 40, 255),
        .btn_theme_special_dark = RGBA8(110, 18, 25, 255),
        .text_primary = RGBA8(240, 242, 245, 255),
        .text_muted = RGBA8(150, 155, 165, 255),
        .text_accent = RGBA8(220, 180, 70, 255),
        .spool_hub = RGBA8(235, 235, 235, 255),
        .spool_gear = RGBA8(150, 155, 165, 255),
        .tape_brown = RGBA8(50, 36, 26, 255),
    },
    [THEME_STEALTH_OLED] = {
        .name = "Stealth AMOLED",
        .model_stamp = "PSVITAMAN - OLED STEALTH HIGH BIAS",
        .bg_chassis = RGBA8(0, 0, 0, 255),
        .border_light = RGBA8(32, 38, 50, 255),
        .border_dark = RGBA8(0, 0, 0, 255),
        .cassette_shell = RGBA8(8, 10, 14, 255),
        .cassette_inner = RGBA8(0, 0, 0, 255),
        .label_bg = RGBA8(12, 16, 22, 255),
        .label_stripe_a = RGBA8(0, 230, 118, 255),
        .label_stripe_b = RGBA8(0, 176, 255, 255),
        .label_text = RGBA8(0, 230, 118, 255),
        .btn_normal = RGBA8(14, 18, 26, 255),
        .btn_pressed = RGBA8(4, 6, 10, 255),
        .btn_border_hi = RGBA8(45, 55, 75, 255),
        .btn_border_lo = RGBA8(0, 0, 0, 255),
        .btn_active_led = RGBA8(0, 230, 118, 255),
        .btn_theme_special = RGBA8(0, 230, 118, 255),
        .btn_theme_special_dark = RGBA8(0, 120, 60, 255),
        .text_primary = RGBA8(240, 245, 250, 255),
        .text_muted = RGBA8(90, 105, 125, 255),
        .text_accent = RGBA8(0, 230, 118, 255),
        .spool_hub = RGBA8(220, 225, 235, 255),
        .spool_gear = RGBA8(0, 176, 255, 255),
        .tape_brown = RGBA8(30, 22, 16, 255),
    }
};

static int s_current_theme = THEME_TPS_L2;

void ui_set_theme(int theme_id) {
    if (theme_id >= 0 && theme_id < THEME_COUNT) {
        s_current_theme = theme_id;
    }
}

int ui_get_theme(void) {
    return s_current_theme;
}

void ui_cycle_theme(void) {
    s_current_theme = (s_current_theme + 1) % THEME_COUNT;
}

void ui_cycle_theme_prev(void) {
    s_current_theme = (s_current_theme + THEME_COUNT - 1) % THEME_COUNT;
}

const char *ui_get_theme_name(int theme_id) {
    if (theme_id >= 0 && theme_id < THEME_COUNT) {
        return s_themes[theme_id].name;
    }
    return "Unknown";
}

static vita2d_pgf *s_font = NULL;

/* Tape Reel Physics & Animation State */
static float s_left_spool_angle = 0.0f;
static float s_right_spool_angle = 0.0f;
static float s_playback_speed = 0.0f;         /* Motor spin-up / spin-down inertia: 0.0 -> 1.0 */
static float s_fast_seek_boost = 0.0f;        /* High-speed whir burst on track skip / seek */
static float s_flutter_time = 0.0f;           /* Wow & flutter mechanical phase */
static float s_tape_counter_rotations = 0.0f; /* Mechanical counter driven by take-up reel */
static int s_prev_progress_ms = 0;
static float s_marquee_offset = 0.0f;
static char s_prev_track[SPOTIFY_TRACK_NAME_MAX] = {0};

/* AMOLED Burn-In Prevention State (Periodic Pixel Orbiting) */
static float s_burn_in_timer = 0.0f;
static int s_orbit_idx = 0;
static int s_shift_x = 0;
static int s_shift_y = 0;
static const int s_orbit_x[8] = { 0,  1,  2,  1,  0, -1, -2, -1 };
static const int s_orbit_y[8] = { 0,  1,  0, -1, -2, -1,  0,  1 };

bool ui_init(void) {
    s_font = vita2d_load_default_pgf();
    return (s_font != NULL);
}

void ui_cleanup(void) {
    if (s_font) {
        vita2d_free_pgf(s_font);
        s_font = NULL;
    }
}

void ui_update(float delta_time, const SpotifyPlaybackState *state, int interpolated_progress_ms) {
    /* 1. Track change & seek detection -> trigger authentic fast-forward whir burst */
    if (state && strcmp(state->track_name, s_prev_track) != 0) {
        utils_safe_strncpy(s_prev_track, state->track_name, sizeof(s_prev_track));
        s_marquee_offset = 0.0f;
        s_fast_seek_boost = 3.5f; /* 3.5x speed burst */
    }

    if (s_prev_progress_ms > 0 && abs(interpolated_progress_ms - s_prev_progress_ms) > 2500) {
        s_fast_seek_boost = 3.0f; /* Fast whir on track seek / skip */
    }
    s_prev_progress_ms = interpolated_progress_ms;

    if (s_fast_seek_boost > 0.0f) {
        s_fast_seek_boost -= 5.0f * delta_time;
        if (s_fast_seek_boost < 0.0f) s_fast_seek_boost = 0.0f;
    }

    /* 2. Motor Inertia (smooth mechanical spin-up & spin-down) */
    float target_speed = (state && state->is_playing) ? 1.0f : 0.0f;
    float motor_accel = (target_speed > s_playback_speed) ? 9.0f : 5.0f;
    s_playback_speed += (target_speed - s_playback_speed) * motor_accel * delta_time;
    if (s_playback_speed < 0.001f) s_playback_speed = 0.0f;

    /* 3. Wow & Flutter (mechanical belt-drive micro-harmonics) */
    s_flutter_time += delta_time;
    if (s_flutter_time >= 62.83f) s_flutter_time -= 62.83f;
    float flutter = 1.0f + 0.022f * sinf(s_flutter_time * 4.2f) + 0.010f * cosf(s_flutter_time * 11.8f);

    float effective_speed = (s_playback_speed + s_fast_seek_boost) * flutter;

    /* 4. Dynamic Reel Angular Velocities (Linear Velocity Conservation v = w * r) */
    float progress_ratio = 0.0f;
    if (state && state->duration_ms > 0) {
        progress_ratio = (float)interpolated_progress_ms / (float)state->duration_ms;
        if (progress_ratio < 0.0f) progress_ratio = 0.0f;
        if (progress_ratio > 1.0f) progress_ratio = 1.0f;
    }

    /* Exact cross-sectional tape area conservation: r = sqrt(r_min^2 + (r_max^2 - r_min^2) * ratio) */
    const float min_r = 26.0f;
    const float max_r = 66.0f;
    const float min_r_sq = min_r * min_r;
    const float max_r_sq = max_r * max_r;
    float left_radius  = sqrtf(min_r_sq + (max_r_sq - min_r_sq) * (1.0f - progress_ratio));
    float right_radius = sqrtf(min_r_sq + (max_r_sq - min_r_sq) * progress_ratio);

    if (effective_speed > 0.001f) {
        /* Base constant calibrated for standard 4.76 cm/s tape transport visual speed */
        const float K = 5200.0f; /* deg*px / s */
        float omega_left = (K / left_radius) * effective_speed;
        float omega_right = (K / right_radius) * effective_speed;

        /* Both spools rotate counter-clockwise */
        s_left_spool_angle -= omega_left * delta_time;
        s_right_spool_angle -= omega_right * delta_time;

        /* Mechanical counter geared to take-up reel rotation */
        s_tape_counter_rotations += (omega_right * delta_time) / 360.0f;

        if (s_left_spool_angle <= -360.0f) s_left_spool_angle += 360.0f;
        if (s_right_spool_angle <= -360.0f) s_right_spool_angle += 360.0f;
    }

    /* 5. Marquee scroll animation */
    s_marquee_offset += 45.0f * delta_time;

    /* 6. AMOLED Burn-In Orbit: shift canvas every 45 seconds */
    s_burn_in_timer += delta_time;
    if (s_burn_in_timer >= 45.0f) {
        s_burn_in_timer = 0.0f;
        s_orbit_idx = (s_orbit_idx + 1) % 8;
        s_shift_x = s_orbit_x[s_orbit_idx];
        s_shift_y = s_orbit_y[s_orbit_idx];
    }
}

static void draw_beveled_box(float x, float y, float w, float h,
                             unsigned int bg, unsigned int border_hi, unsigned int border_lo) {
    vita2d_draw_rectangle(x, y, w, h, bg);
    /* Top & Left highlight */
    vita2d_draw_line(x, y, x + w, y, border_hi);
    vita2d_draw_line(x, y + 1, x + w - 1, y + 1, border_hi);
    vita2d_draw_line(x, y, x, y + h, border_hi);
    vita2d_draw_line(x + 1, y, x + 1, y + h - 1, border_hi);
    /* Bottom & Right shadow */
    vita2d_draw_line(x, y + h - 1, x + w, y + h - 1, border_lo);
    vita2d_draw_line(x + 1, y + h - 2, x + w, y + h - 2, border_lo);
    vita2d_draw_line(x + w - 1, y, x + w - 1, y + h, border_lo);
    vita2d_draw_line(x + w - 2, y + 1, x + w - 2, y + h, border_lo);
}

static void draw_triangle_right(float x, float y, float w, float h, unsigned int c) {
    for (float i = 0; i < w; i += 1.0f) {
        float half_h = (h * 0.5f) * ((w - i) / w);
        vita2d_draw_line(x + i, y - half_h, x + i, y + half_h, c);
    }
}

static void draw_triangle_left(float x, float y, float w, float h, unsigned int c) {
    for (float i = 0; i < w; i += 1.0f) {
        float half_h = (h * 0.5f) * (i / w);
        vita2d_draw_line(x + i, y - half_h, x + i, y + half_h, c);
    }
}

static void draw_icon_play(float cx, float cy, float size, unsigned int c) {
    draw_triangle_right(cx - size * 0.35f, cy, size * 0.80f, size, c);
}

static void draw_icon_pause(float cx, float cy, float w, float h, unsigned int c) {
    float bar_w = w * 0.30f;
    float gap = w * 0.35f;
    vita2d_draw_rectangle(cx - gap * 0.5f - bar_w, cy - h * 0.5f, bar_w, h, c);
    vita2d_draw_rectangle(cx + gap * 0.5f, cy - h * 0.5f, bar_w, h, c);
}

static void draw_icon_prev(float cx, float cy, float size, unsigned int c) {
    float tri_w = size * 0.45f;
    draw_triangle_left(cx - tri_w, cy, tri_w, size, c);
    draw_triangle_left(cx, cy, tri_w, size, c);
}

static void draw_icon_next(float cx, float cy, float size, unsigned int c) {
    float tri_w = size * 0.45f;
    draw_triangle_right(cx - tri_w, cy, tri_w, size, c);
    draw_triangle_right(cx, cy, tri_w, size, c);
}

static void draw_icon_shuffle(float cx, float cy, float size, unsigned int c) {
    float r = size * 0.40f;
    vita2d_draw_line(cx - r, cy - r * 0.5f, cx + r * 0.3f, cy + r * 0.5f, c);
    vita2d_draw_line(cx - r, cy - r * 0.5f + 1, cx + r * 0.3f, cy + r * 0.5f + 1, c);
    vita2d_draw_line(cx - r, cy + r * 0.5f, cx + r * 0.3f, cy - r * 0.5f, c);
    vita2d_draw_line(cx - r, cy + r * 0.5f + 1, cx + r * 0.3f, cy - r * 0.5f + 1, c);
    draw_triangle_right(cx + r * 0.3f, cy - r * 0.5f, r * 0.5f, r * 0.6f, c);
    draw_triangle_right(cx + r * 0.3f, cy + r * 0.5f, r * 0.5f, r * 0.6f, c);
}

static void draw_icon_repeat(float cx, float cy, float size, bool is_track, unsigned int c) {
    float r = size * 0.42f;
    vita2d_draw_line(cx - r, cy - r * 0.4f, cx + r * 0.4f, cy - r * 0.4f, c);
    vita2d_draw_line(cx + r * 0.4f, cy - r * 0.4f, cx + r * 0.4f, cy + r * 0.4f, c);
    vita2d_draw_line(cx + r * 0.4f, cy + r * 0.4f, cx - r * 0.4f, cy + r * 0.4f, c);
    vita2d_draw_line(cx - r * 0.4f, cy + r * 0.4f, cx - r * 0.4f, cy - r * 0.1f, c);
    draw_triangle_right(cx + r * 0.3f, cy - r * 0.4f, r * 0.45f, r * 0.55f, c);
    if (is_track && s_font) {
        vita2d_pgf_draw_text(s_font, (int)(cx - 3), (int)(cy + 4), c, 0.65f, "1");
    }
}

static void draw_icon_theme(float cx, float cy, float size, unsigned int fill_c, unsigned int inner_c) {
    /* Walkman distinctive concentric circular button recess */
    vita2d_draw_fill_circle(cx, cy, size * 0.46f, inner_c);
    vita2d_draw_fill_circle(cx, cy, size * 0.30f, fill_c);
    vita2d_draw_fill_circle(cx, cy, size * 0.14f, inner_c);
}

static void draw_screw(float x, float y) {
    vita2d_draw_fill_circle(x, y, 6, RGBA8(170, 175, 185, 255));
    vita2d_draw_fill_circle(x, y, 4, RGBA8(140, 145, 155, 255));
    vita2d_draw_line(x - 4, y, x + 4, y, RGBA8(70, 75, 85, 255));
}

static void draw_spool(float cx, float cy, float outer_radius, float angle_deg, bool is_leader, const AppTheme *theme) {
    /* 1. Outer Spooled Magnetic Tape Pack */
    if (outer_radius > 26.5f) {
        unsigned int tape_col = is_leader ? RGBA8(215, 190, 190, 220) : theme->tape_brown;
        vita2d_draw_fill_circle(cx, cy, outer_radius, tape_col);

        /* Dense concentric tape pack winding layers */
        for (float r = 29.5f; r < outer_radius - 1.5f; r += 3.5f) {
            unsigned int layer_col = is_leader
                ? RGBA8(235, 215, 215, 160)
                : ((theme->tape_brown & 0x00FFFFFF) | 0x22000000);
            vita2d_draw_fill_circle(cx, cy, r, layer_col);
        }

        /* Subtle tape pack rim shadow */
        vita2d_draw_fill_circle(cx, cy, outer_radius, 0x18000000);

        /* Specular light sheen on tightly wound tape pack (top-right quadrant) */
        float sheen_dist = (outer_radius + 26.0f) * 0.32f;
        float sheen_rad  = (outer_radius - 26.0f) * 0.36f;
        vita2d_draw_fill_circle(cx + sheen_dist, cy - sheen_dist, sheen_rad, RGBA8(255, 255, 255, 16));
    }

    /* 2. Outer Plastic Spool Flange Rim */
    vita2d_draw_fill_circle(cx, cy, 27.0f, RGBA8(218, 222, 228, 255));

    /* 3. White Molded Plastic Cassette Hub Core */
    vita2d_draw_fill_circle(cx, cy, 25.0f, theme->spool_hub);
    vita2d_draw_fill_circle(cx, cy, 14.0f, theme->cassette_inner);

    /* 4. Six Molded Drive Teeth with Mechanical Depth */
    for (int i = 0; i < 6; i++) {
        float rad = (angle_deg + i * 60.0f) * (3.14159265f / 180.0f);
        float cos_a = cosf(rad);
        float sin_a = sinf(rad);

        float x1 = cx + cos_a * 12.5f;
        float y1 = cy + sin_a * 12.5f;
        float x2 = cx + cos_a * 25.0f;
        float y2 = cy + sin_a * 25.0f;

        /* Molded tooth spoke */
        vita2d_draw_line(x1, y1, x2, y2, theme->spool_gear);

        /* Tooth thickness & bevel highlights */
        float px = -sin_a * 1.5f;
        float py = cos_a * 1.5f;
        vita2d_draw_line(x1 + px, y1 + py, x2 + px, y2 + py, theme->spool_gear);
        vita2d_draw_line(x1 - px, y1 - py, x2 - px, y2 - py, RGBA8(240, 245, 250, 180));
    }

    /* 5. Center Steel Drive Spindle Pin with Specular Highlight */
    vita2d_draw_fill_circle(cx, cy, 4.5f, RGBA8(195, 200, 210, 255));
    vita2d_draw_fill_circle(cx, cy, 2.0f, RGBA8(65, 70, 80, 255));
    vita2d_draw_fill_circle(cx - 1.0f, cy - 1.0f, 1.0f, RGBA8(255, 255, 255, 220));
}

static void draw_tape_counter(float kx, float ky, int counter_val, const AppTheme *theme) {
    float kw = 48.0f;
    float kh = 30.0f;
    draw_beveled_box(kx, ky, kw, kh, RGBA8(10, 12, 16, 255), theme->border_dark, theme->border_light);

    if (s_font) {
        /* "COUNTER" label above */
        vita2d_pgf_draw_text(s_font, (int)kx, (int)(ky - 5), theme->text_muted, 0.55f, "COUNTER");

        char c_buf[8];
        snprintf(c_buf, sizeof(c_buf), "%03d", counter_val % 1000);

        /* 3 rotary digit slots */
        for (int i = 0; i < 3; i++) {
            float wx = kx + 3.0f + i * 14.0f;
            vita2d_draw_rectangle(wx, ky + 4.0f, 13.0f, 22.0f, RGBA8(22, 25, 32, 255));
            vita2d_draw_line(wx, ky + 4.0f, wx, ky + 26.0f, RGBA8(40, 45, 55, 255));
            vita2d_draw_line(wx + 12.0f, ky + 4.0f, wx + 12.0f, ky + 26.0f, RGBA8(5, 7, 10, 255));

            char d_str[2] = { c_buf[i], '\0' };
            int dw = vita2d_pgf_text_width(s_font, 0.78f, d_str);
            vita2d_pgf_draw_text(s_font, (int)(wx + (13.0f - dw) * 0.5f), (int)(ky + 21.0f), RGBA8(240, 242, 245, 255), 0.78f, d_str);
        }

        /* Miniature reset push-button */
        vita2d_draw_fill_circle(kx + kw + 5.0f, ky + kh * 0.5f, 2.5f, RGBA8(230, 235, 240, 255));
        vita2d_draw_fill_circle(kx + kw + 5.0f, ky + kh * 0.5f, 1.0f, RGBA8(180, 185, 190, 255));
    }
}

static void draw_vu_meters(float vx, float vy, bool is_playing, float anim_phase, const AppTheme *theme) {
    float vw = 48.0f;
    float vh = 96.0f;
    draw_beveled_box(vx, vy, vw, vh, RGBA8(12, 14, 18, 255), theme->border_dark, theme->border_light);

    if (s_font) {
        vita2d_pgf_draw_text(s_font, (int)(vx + 2), (int)(vy - 5), theme->text_muted, 0.55f, "LEVEL dB");
        vita2d_pgf_draw_text(s_font, (int)(vx + 8), (int)(vy + 13), theme->text_muted, 0.52f, "L");
        vita2d_pgf_draw_text(s_font, (int)(vx + 29), (int)(vy + 13), theme->text_muted, 0.52f, "R");
    }

    int lit_l = 0;
    int lit_r = 0;
    if (is_playing) {
        float s1 = fabsf(sinf(anim_phase * 0.12f));
        float s2 = fabsf(cosf(anim_phase * 0.17f + 0.5f));
        lit_l = 1 + (int)(s1 * 4.99f);
        lit_r = 1 + (int)(s2 * 4.99f);
    }

    /* 6 horizontal LED bars per channel */
    for (int i = 0; i < 6; i++) {
        float bar_y = vy + 78.0f - i * 11.0f;
        unsigned int col_active;
        unsigned int col_inactive;

        if (i < 3) {
            col_active = RGBA8(30, 215, 96, 255);
            col_inactive = RGBA8(12, 45, 22, 255);
        } else if (i < 5) {
            col_active = RGBA8(255, 195, 25, 255);
            col_inactive = RGBA8(55, 42, 8, 255);
        } else {
            col_active = RGBA8(235, 45, 45, 255);
            col_inactive = RGBA8(55, 12, 12, 255);
        }

        vita2d_draw_rectangle(vx + 5.0f, bar_y, 16.0f, 7.0f, (i < lit_l) ? col_active : col_inactive);
        vita2d_draw_rectangle(vx + 27.0f, bar_y, 16.0f, 7.0f, (i < lit_r) ? col_active : col_inactive);
    }
}

static void render_top_hud(const SpotifyPlaybackState *state, const AppTheme *theme, int ox, int oy, bool is_syncing) {
    float tx = 24.0f + ox;
    float ty = 8.0f + oy;
    float tw = 912.0f;
    float th = 42.0f;

    draw_beveled_box(tx, ty, tw, th, theme->border_dark, theme->border_light, theme->border_dark);

    /* Subtle brushed metal accent lines */
    for (int y = (int)(ty + 4); y < (int)(ty + th - 4); y += 6) {
        vita2d_draw_line(tx + 4, y, tx + tw - 4, y, (theme->border_light & 0x00FFFFFF) | 0x18000000);
    }

    if (s_font) {
        /* Bold SONY brand block */
        vita2d_pgf_draw_text(s_font, (int)(tx + 16), (int)(ty + 27), theme->text_primary, 0.88f, "SONY");

        /* Model Stamp */
        vita2d_pgf_draw_text(s_font, (int)(tx + 76), (int)(ty + 27), theme->text_accent, 0.68f, theme->model_stamp);

        /* Spotify Device Name (clipped safely in center column) */
        char dev_hud[128];
        snprintf(dev_hud, sizeof(dev_hud), "DEV: %s", (strlen(state->device_name) > 0) ? state->device_name : "Idle");
        vita2d_set_clip_rectangle((int)(tx + 360), (int)(ty + 6), 250, 30);
        vita2d_pgf_draw_text(s_font, (int)(tx + 360), (int)(ty + 27), theme->text_muted, 0.70f, dev_hud);
        vita2d_disable_clipping();

        /* Headphone Jacks motif (authentic dual 3.5mm Walkman sockets) */
        vita2d_pgf_draw_text(s_font, (int)(tx + 630), (int)(ty + 27), theme->text_muted, 0.62f, "PHONES");
        vita2d_draw_fill_circle(tx + 704, ty + 19, 4.5f, RGBA8(180, 185, 195, 255));
        vita2d_draw_fill_circle(tx + 704, ty + 19, 2.0f, RGBA8(20, 24, 30, 255));
        vita2d_draw_fill_circle(tx + 722, ty + 19, 4.5f, RGBA8(180, 185, 195, 255));
        vita2d_draw_fill_circle(tx + 722, ty + 19, 2.0f, RGBA8(20, 24, 30, 255));

        /* Volume Display */
        char vol_hud[32];
        snprintf(vol_hud, sizeof(vol_hud), "VOL %d%%", state->volume_percent);
        vita2d_pgf_draw_text(s_font, (int)(tx + 750), (int)(ty + 27), theme->text_primary, 0.74f, vol_hud);

        /* 5-Bar Volume Graphic */
        int num_bars = (state->volume_percent * 5 + 50) / 100;
        for (int b = 0; b < 5; b++) {
            float bx = tx + 828 + b * 6.0f;
            float bh = 4.0f + b * 2.5f;
            unsigned int bc = (b < num_bars) ? theme->btn_active_led : RGBA8(60, 70, 85, 255);
            vita2d_draw_rectangle(bx, ty + 25.0f - bh, 4.0f, bh, bc);
        }

        /* Live Sync Pulse Dot */
        unsigned int sync_c = is_syncing ? theme->btn_active_led : RGBA8(60, 70, 85, 255);
        vita2d_draw_fill_circle(tx + tw - 18, ty + 19, 4.5f, sync_c);
    }
}

static void render_cassette_bay(const SpotifyPlaybackState *state, int interpolated_progress_ms, const AppTheme *theme, int ox, int oy) {
    /* Outer Cassette Door Frame */
    float cx = 24.0f + ox;
    float cy = 56.0f + oy;
    float cw = 912.0f;
    float ch = 368.0f;

    draw_beveled_box(cx, cy, cw, ch, theme->bg_chassis, theme->border_light, theme->border_dark);

    /* Corner Screws on outer door */
    draw_screw(cx + 10, cy + 10);
    draw_screw(cx + cw - 10, cy + 10);
    draw_screw(cx + 10, cy + ch - 10);
    draw_screw(cx + cw - 10, cy + ch - 10);

    /* Stamped Header Text on outer brushed metal door */
    if (s_font) {
        vita2d_pgf_draw_text(s_font, (int)(cx + 24), (int)(cy + 16), theme->text_muted, 0.58f, "EJECT [SELECT]");
        vita2d_pgf_draw_text(s_font, (int)(cx + cw - 105), (int)(cy + 16), theme->text_muted, 0.58f, "DIRECTION >>");
    }

    /* Cassette Tape Shell Body */
    float sx = cx + 18.0f;
    float sy = cy + 22.0f;
    float sw = cw - 36.0f; /* 876.0f */
    float sh = 340.0f;

    draw_beveled_box(sx, sy, sw, sh, theme->cassette_shell, theme->border_light, theme->border_dark);

    /* 5 Cassette Shell Assembly Screws */
    draw_screw(sx + 14, sy + 14);
    draw_screw(sx + sw - 14, sy + 14);
    draw_screw(sx + 14, sy + sh - 14);
    draw_screw(sx + sw - 14, sy + sh - 14);
    draw_screw(sx + sw * 0.5f, sy + 14);

    /* Upper Cassette Paper Label Card */
    float lx = sx + 24.0f;
    float ly = sy + 8.0f;
    float lw = sw - 48.0f; /* 828.0f */
    float lh = 94.0f;

    draw_beveled_box(lx, ly, lw, lh, theme->label_bg, RGBA8(255, 255, 255, 240), RGBA8(180, 175, 165, 255));

    /* Dual Racing Stripes across Label */
    vita2d_draw_rectangle(lx + 2, ly + 5, lw - 4, 3, theme->label_stripe_a);
    vita2d_draw_rectangle(lx + 2, ly + 8, lw - 4, 3, theme->label_stripe_b);

    if (s_font) {
        /* Bold Boxed "A" */
        draw_beveled_box(lx + 12, ly + 15, 20, 18, theme->label_stripe_a, RGBA8(255, 255, 255, 255), RGBA8(0, 0, 0, 255));
        vita2d_pgf_draw_text(s_font, (int)(lx + 16), (int)(ly + 29), RGBA8(255, 255, 255, 255), 0.80f, "A");

        /* Label Typography */
        vita2d_pgf_draw_text(s_font, (int)(lx + 38), (int)(ly + 28), theme->label_text, 0.65f, "PSVITAMAN - C-90 HIGH BIAS 70us EQ");
        vita2d_pgf_draw_text(s_font, (int)(lx + lw - 190), (int)(ly + 28), theme->label_text, 0.65f, "DOLBY B NR  |  STEREO");

        /* Track Title Marquee */
        const char *track_title = (strlen(state->track_name) > 0) ? state->track_name : "Playback Idle / Stopped";
        int title_w = vita2d_pgf_text_width(s_font, 1.15f, track_title);

        float clip_x = lx + 12.0f;
        float clip_y = ly + 33.0f;
        float clip_w = lw - 24.0f;
        float clip_h = 32.0f;

        vita2d_set_clip_rectangle((int)clip_x, (int)clip_y, (int)clip_w, (int)clip_h);

        if (title_w > clip_w) {
            float total_scroll = title_w + 80.0f;
            float cur_x = clip_x - fmodf(s_marquee_offset, total_scroll);
            vita2d_pgf_draw_text(s_font, (int)cur_x, (int)(clip_y + 24), theme->label_text, 1.15f, track_title);
            vita2d_pgf_draw_text(s_font, (int)(cur_x + total_scroll), (int)(clip_y + 24), theme->label_text, 1.15f, track_title);
        } else {
            vita2d_pgf_draw_text(s_font, (int)clip_x, (int)(clip_y + 24), theme->label_text, 1.15f, track_title);
        }
        vita2d_disable_clipping();

        /* Artist & Album Subtitle */
        char artist_album[600] = {0};
        if (strlen(state->artist_name) > 0) {
            if (strlen(state->album_name) > 0) {
                snprintf(artist_album, sizeof(artist_album), "%s - %s", state->artist_name, state->album_name);
            } else {
                utils_safe_strncpy(artist_album, state->artist_name, sizeof(artist_album));
            }
        } else {
            utils_safe_strncpy(artist_album, "Connect Spotify from Phone, PC, or Console", sizeof(artist_album));
        }

        vita2d_set_clip_rectangle((int)clip_x, (int)(ly + 68), (int)clip_w, 22);
        vita2d_pgf_draw_text(s_font, (int)clip_x, (int)(ly + 84), RGBA8(80, 85, 95, 255), 0.76f, artist_album);
        vita2d_disable_clipping();
    }

    /* Central Clear Acrylic Cassette Window */
    float wx = sx + 74.0f;
    float wy = sy + 108.0f;
    float ww = sw - 148.0f; /* 728.0f */
    float wh = 158.0f;

    /* Deep Sunken Acrylic Window Cavity */
    draw_beveled_box(wx, wy, ww, wh, theme->cassette_inner, theme->border_dark, theme->border_light);

    /* Mechanical Tape Counter (Left of Window) - geared to right spool rotation */
    int counter_val = (int)(s_tape_counter_rotations * 1.5f + (float)interpolated_progress_ms * 0.001f * 0.8f) % 1000;
    draw_tape_counter(sx + 10.0f, wy + 40.0f, counter_val, theme);

    /* Stereo LED VU Meters (Right of Window) - synchronized to flutter/playback phase */
    draw_vu_meters(sx + sw - 60.0f, wy + 18.0f, state->is_playing, s_flutter_time, theme);

    /* Stamped "AUTO STOP" Header inside window glass */
    if (s_font) {
        const char *auto_stop = "AUTO  STOP";
        int asw = vita2d_pgf_text_width(s_font, 0.65f, auto_stop);
        vita2d_pgf_draw_text(s_font, (int)(wx + (ww - asw) * 0.5f), (int)(wy + 18), (theme->border_light & 0x00FFFFFF) | 0x99000000, 0.65f, auto_stop);
    }

    /* Physics-inspired tape roll geometry (Conservation of Cross-Sectional Tape Area) */
    float progress_ratio = 0.0f;
    if (state->duration_ms > 0) {
        progress_ratio = (float)interpolated_progress_ms / (float)state->duration_ms;
        if (progress_ratio < 0.0f) progress_ratio = 0.0f;
        if (progress_ratio > 1.0f) progress_ratio = 1.0f;
    }

    const float min_r = 26.0f;
    const float max_r = 66.0f;
    const float min_r_sq = min_r * min_r;
    const float max_r_sq = max_r * max_r;
    float left_radius  = sqrtf(min_r_sq + (max_r_sq - min_r_sq) * (1.0f - progress_ratio));
    float right_radius = sqrtf(min_r_sq + (max_r_sq - min_r_sq) * progress_ratio);

    float left_cx  = wx + 135.0f;
    float right_cx = wx + ww - 135.0f;
    float spool_cy = wy + wh * 0.5f - 2.0f;

    /* Dual Rotating Cassette Spools with Dynamic Linear Speed Physics */
    bool left_is_leader = (progress_ratio > 0.985f);
    bool right_is_leader = (progress_ratio < 0.015f);
    draw_spool(left_cx, spool_cy, left_radius, s_left_spool_angle, left_is_leader, theme);
    draw_spool(right_cx, spool_cy, right_radius, s_right_spool_angle, right_is_leader, theme);

    /* Tangential Tape Ribbon Path: peeling from supply reel, across rollers, into take-up reel */
    float roller_y = spool_cy + 52.0f;
    float roller_L_x = left_cx - 38.0f;
    float roller_R_x = right_cx + 38.0f;

    /* Left unspooling ribbon: peels tangentially from outer edge of supply reel */
    float peel_L_x = left_cx - left_radius + 3.0f;
    float peel_L_y = spool_cy + left_radius * 0.42f;
    for (int t = -2; t <= 2; t++) {
        vita2d_draw_line(peel_L_x + t, peel_L_y, roller_L_x + t, roller_y - 2.0f, theme->tape_brown);
    }

    /* Horizontal magnetic tape ribbon across capstan & playback head notch */
    vita2d_draw_rectangle(roller_L_x - 3.0f, roller_y - 3.0f, (roller_R_x - roller_L_x) + 6.0f, 6.0f, theme->tape_brown);
    /* Specular oxide sheen line along tape run */
    vita2d_draw_line(roller_L_x, roller_y - 1.0f, roller_R_x, roller_y - 1.0f, (theme->tape_brown & 0x00FFFFFF) | 0x44000000);

    /* Right intake ribbon: feeds tangentially into outer edge of take-up reel */
    float peel_R_x = right_cx + right_radius - 3.0f;
    float peel_R_y = spool_cy + right_radius * 0.42f;
    for (int t = -2; t <= 2; t++) {
        vita2d_draw_line(roller_R_x + t, roller_y - 2.0f, peel_R_x + t, peel_R_y, theme->tape_brown);
    }

    /* Left & Right Flanged Tape Guide Rollers (drawn over ribbon for authentic mechanical wrap) */
    vita2d_draw_fill_circle(roller_L_x, roller_y, 6.0f, RGBA8(195, 200, 210, 255));
    vita2d_draw_fill_circle(roller_L_x, roller_y, 3.5f, RGBA8(110, 115, 125, 255));
    vita2d_draw_fill_circle(roller_L_x, roller_y, 1.5f, RGBA8(240, 245, 250, 255));

    vita2d_draw_fill_circle(roller_R_x, roller_y, 6.0f, RGBA8(195, 200, 210, 255));
    vita2d_draw_fill_circle(roller_R_x, roller_y, 3.5f, RGBA8(110, 115, 125, 255));
    vita2d_draw_fill_circle(roller_R_x, roller_y, 1.5f, RGBA8(240, 245, 250, 255));

    /* Center tape index scale lines on acrylic window */
    float center_x = wx + ww * 0.5f;
    if (s_font) {
        vita2d_pgf_draw_text(s_font, (int)(center_x - 34), (int)(spool_cy - 24), (theme->border_light & 0x00FFFFFF) | 0x88000000, 0.58f, "100  50   0");
    }
    for (int i = -3; i <= 3; i++) {
        float mark_y = spool_cy + i * 7.0f;
        vita2d_draw_line(center_x - 14, mark_y, center_x + 14, mark_y, (theme->border_light & 0x00FFFFFF) | 0x77000000);
    }

    /* Acrylic Window Glare / Glass Reflection Highlight */
    vita2d_draw_line(wx + 30, wy + wh - 12, wx + 140, wy + 12, RGBA8(255, 255, 255, 24));
    vita2d_draw_line(wx + 32, wy + wh - 12, wx + 142, wy + 12, RGBA8(255, 255, 255, 36));
    vita2d_draw_line(wx + 34, wy + wh - 12, wx + 144, wy + 12, RGBA8(255, 255, 255, 24));

    /* Cassette Head Trapezoid Notch */
    float tz_w = 380.0f;
    float tz_x = sx + (sw - tz_w) * 0.5f;
    float tz_y = sy + sh - 68.0f;
    float tz_h = 26.0f;
    draw_beveled_box(tz_x, tz_y, tz_w, tz_h, theme->cassette_inner, theme->border_dark, theme->border_light);

    /* Capstan Holes */
    vita2d_draw_fill_circle(tz_x + 50.0f, tz_y + 13.0f, 6.0f, RGBA8(180, 185, 195, 255));
    vita2d_draw_fill_circle(tz_x + 50.0f, tz_y + 13.0f, 3.0f, RGBA8(10, 12, 16, 255));
    vita2d_draw_fill_circle(tz_x + tz_w - 50.0f, tz_y + 13.0f, 6.0f, RGBA8(180, 185, 195, 255));
    vita2d_draw_fill_circle(tz_x + tz_w - 50.0f, tz_y + 13.0f, 3.0f, RGBA8(10, 12, 16, 255));

    /* Tape Head Exposure Window & Magnetic Ribbon */
    vita2d_draw_rectangle(tz_x + 95.0f, tz_y + 6.0f, tz_w - 190.0f, 14.0f, RGBA8(8, 10, 14, 255));
    vita2d_draw_rectangle(tz_x + 105.0f, tz_y + 10.0f, tz_w - 210.0f, 6.0f, theme->tape_brown);

    /* Lower HUD Inset Strip: Progress Bar & Time Readout */
    float hx = sx + 24.0f;
    float hy = sy + sh - 36.0f;
    float hw = sw - 48.0f; /* 828.0f */
    float hh = 26.0f;

    draw_beveled_box(hx, hy, hw, hh, RGBA8(14, 16, 22, 255), theme->border_dark, theme->border_light);

    /* Clean Progress Bar in Center of Strip */
    float pb_x = hx + 175.0f;
    float pb_w = hw - 370.0f;
    vita2d_draw_rectangle(pb_x, hy + 11.0f, pb_w, 4.0f, RGBA8(28, 32, 42, 255));
    if (progress_ratio > 0.0f) {
        float filled_w = pb_w * progress_ratio;
        vita2d_draw_rectangle(pb_x, hy + 11.0f, filled_w, 4.0f, theme->btn_active_led);
        vita2d_draw_fill_circle(pb_x + filled_w, hy + 13.0f, 4.0f, theme->btn_active_led);
    }

    if (s_font) {
        /* Time Readout (Left): MM:SS / MM:SS */
        char cur_time[16], total_time[16], time_hud[40];
        utils_format_time_ms(interpolated_progress_ms, cur_time, sizeof(cur_time));
        utils_format_time_ms(state->duration_ms, total_time, sizeof(total_time));
        snprintf(time_hud, sizeof(time_hud), "%s / %s", cur_time, total_time);
        vita2d_pgf_draw_text(s_font, (int)(hx + 12), (int)(hy + 18), theme->btn_active_led, 0.74f, time_hud);

        /* Mode Badges (Right): SHUF & REP */
        char mode_str[64];
        const char *rep_str = (state->repeat_state == REPEAT_TRACK) ? "TRK" : ((state->repeat_state == REPEAT_CONTEXT) ? "ALL" : "OFF");
        snprintf(mode_str, sizeof(mode_str), "SHUF: %s | REP: %s", state->shuffle_state ? "ON" : "OFF", rep_str);
        int mw = vita2d_pgf_text_width(s_font, 0.68f, mode_str);
        vita2d_pgf_draw_text(s_font, (int)(hx + hw - mw - 12), (int)(hy + 18), theme->text_muted, 0.68f, mode_str);
    }
}

static void render_transport_bar(const SpotifyPlaybackState *state, const InputState *input, const AppTheme *theme, int ox, int oy) {
    /* Sunken mechanical button bay across the bottom of the device */
    float bx_bay = 24.0f + ox;
    float by_bay = 432.0f + oy;
    float bw_bay = 912.0f;
    float bh_bay = 100.0f;
    draw_beveled_box(bx_bay, by_bay, bw_bay, bh_bay, theme->border_dark, theme->border_dark, theme->border_light);

    const DeckButtonRect *btns = input_get_button_rects();

    for (int i = 0; i < DECK_BTN_COUNT; i++) {
        const DeckButtonRect *b = &btns[i];
        bool is_pressed = (input->pressed_button == i);

        float bx = b->x + ox;
        float by = b->y + oy + (is_pressed ? 3 : 0);
        float bw = b->w;
        float bh = b->h - (is_pressed ? 2 : 0);

        bool is_theme_btn = (i == BTN_INDEX_THEME);
        unsigned int fill_color;
        unsigned int hi_color;
        unsigned int lo_color;

        if (is_theme_btn) {
            fill_color = is_pressed ? theme->btn_theme_special_dark : theme->btn_theme_special;
            hi_color = theme->border_light;
            lo_color = theme->btn_theme_special_dark;
        } else {
            fill_color = is_pressed ? theme->btn_pressed : theme->btn_normal;
            hi_color = is_pressed ? theme->btn_border_lo : theme->btn_border_hi;
            lo_color = is_pressed ? theme->btn_border_hi : theme->btn_border_lo;
        }

        draw_beveled_box(bx, by, bw, bh, fill_color, hi_color, lo_color);

        /* Indicator LED in top corner of togglable buttons */
        bool has_led = (i == BTN_INDEX_SHUFFLE || i == BTN_INDEX_REPEAT || i == BTN_INDEX_PLAY_PAUSE);
        bool is_active = false;
        if (i == BTN_INDEX_PLAY_PAUSE && state->is_playing) is_active = true;
        if (i == BTN_INDEX_SHUFFLE && state->shuffle_state) is_active = true;
        if (i == BTN_INDEX_REPEAT && state->repeat_state != REPEAT_OFF) is_active = true;

        if (!is_theme_btn) {
            /* Tactile piano-key horizontal ridges (cleanly terminate before LED) */
            float ridge_end = has_led ? (bx + bw - 26) : (bx + bw - 14);
            vita2d_draw_line(bx + 14, by + 7, ridge_end, by + 7, theme->btn_border_lo);
            vita2d_draw_line(bx + 14, by + 8, ridge_end, by + 8, theme->btn_border_hi);
            vita2d_draw_line(bx + 14, by + 11, ridge_end, by + 11, theme->btn_border_lo);
            vita2d_draw_line(bx + 14, by + 12, ridge_end, by + 12, theme->btn_border_hi);
        }

        if (has_led) {
            unsigned int led_c = is_active ? theme->btn_active_led : RGBA8(45, 52, 62, 255);
            vita2d_draw_fill_circle(bx + bw - 14, by + 10, 3.5f, led_c);
            if (is_active) {
                vita2d_draw_fill_circle(bx + bw - 14, by + 10, 5.5f, (led_c & 0x00FFFFFF) | 0x44000000);
            }
        }

        /* Center coordinates for icon */
        float icx = bx + bw * 0.5f;
        float icy = by + 27.0f;
        unsigned int icon_color = is_active ? theme->btn_active_led : (is_theme_btn ? RGBA8(255, 255, 255, 255) : theme->text_primary);

        switch (i) {
            case BTN_INDEX_THEME:
                draw_icon_theme(icx, icy, 26.0f, theme->btn_theme_special, theme->btn_theme_special_dark);
                break;
            case BTN_INDEX_PLAY_PAUSE:
                if (state->is_playing) {
                    draw_icon_pause(icx, icy, 18.0f, 18.0f, icon_color);
                } else {
                    draw_icon_play(icx, icy, 20.0f, icon_color);
                }
                break;
            case BTN_INDEX_PREV:
                draw_icon_prev(icx, icy, 20.0f, icon_color);
                break;
            case BTN_INDEX_NEXT:
                draw_icon_next(icx, icy, 20.0f, icon_color);
                break;
            case BTN_INDEX_SHUFFLE:
                draw_icon_shuffle(icx, icy, 20.0f, icon_color);
                break;
            case BTN_INDEX_REPEAT:
                draw_icon_repeat(icx, icy, 20.0f, state->repeat_state == REPEAT_TRACK, icon_color);
                break;
        }

        /* Text label & hotkey hint below icon - separated with clean non-overlapping baselines */
        if (s_font) {
            const char *label_text = b->label;
            if (i == BTN_INDEX_PLAY_PAUSE) {
                label_text = state->is_playing ? "PAUSE" : "PLAY";
            } else if (i == BTN_INDEX_REPEAT) {
                if (state->repeat_state == REPEAT_TRACK) label_text = "REP 1";
                else if (state->repeat_state == REPEAT_CONTEXT) label_text = "REPEAT";
                else label_text = "REP OFF";
            } else if (i == BTN_INDEX_THEME) {
                label_text = "HOT LINE";
            }

            int tw = vita2d_pgf_text_width(s_font, 0.66f, label_text);
            int tx = (int)(bx + (bw - tw) * 0.5f);
            int ty = (int)(by + 53);
            unsigned int tc = is_active ? theme->btn_active_led : (is_theme_btn ? RGBA8(255, 255, 255, 255) : theme->text_muted);
            vita2d_pgf_draw_text(s_font, tx, ty, tc, 0.66f, label_text);

            int hw = vita2d_pgf_text_width(s_font, 0.54f, b->hotkey_hint);
            int hx = (int)(bx + (bw - hw) * 0.5f);
            int hy = (int)(by + 71);
            vita2d_pgf_draw_text(s_font, hx, hy, RGBA8(130, 140, 155, 200), 0.54f, b->hotkey_hint);
        }
    }
}

static void draw_qr_code(float start_x, float start_y, const char *text, int max_pixel_size) {
    if (!text || strlen(text) == 0) return;

    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

    bool ok = qrcodegen_encodeText(text, tempBuffer, qrcode, qrcodegen_Ecc_LOW,
                                   qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                                   qrcodegen_Mask_AUTO, true);
    if (!ok) return;

    int qr_modules = qrcodegen_getSize(qrcode);
    int module_px = max_pixel_size / qr_modules;
    if (module_px < 2) module_px = 2;

    int total_qr_px = qr_modules * module_px;
    int quiet_zone = 10;

    /* White card background with quiet zone */
    vita2d_draw_rectangle(start_x - quiet_zone, start_y - quiet_zone,
                          total_qr_px + quiet_zone * 2, total_qr_px + quiet_zone * 2,
                          RGBA8(255, 255, 255, 255));

    /* Outline border */
    vita2d_draw_line(start_x - quiet_zone, start_y - quiet_zone, start_x + total_qr_px + quiet_zone, start_y - quiet_zone, RGBA8(180, 185, 195, 255));
    vita2d_draw_line(start_x - quiet_zone, start_y + total_qr_px + quiet_zone, start_x + total_qr_px + quiet_zone, start_y + total_qr_px + quiet_zone, RGBA8(180, 185, 195, 255));
    vita2d_draw_line(start_x - quiet_zone, start_y - quiet_zone, start_x - quiet_zone, start_y + total_qr_px + quiet_zone, RGBA8(180, 185, 195, 255));
    vita2d_draw_line(start_x + total_qr_px + quiet_zone, start_y - quiet_zone, start_x + total_qr_px + quiet_zone, start_y + total_qr_px + quiet_zone, RGBA8(180, 185, 195, 255));

    /* Render dark modules */
    for (int y = 0; y < qr_modules; y++) {
        for (int x = 0; x < qr_modules; x++) {
            if (qrcodegen_getModule(qrcode, x, y)) {
                vita2d_draw_rectangle(start_x + x * module_px,
                                      start_y + y * module_px,
                                      module_px, module_px,
                                      RGBA8(18, 22, 30, 255));
            }
        }
    }
}

static void render_setup_guide(const AppConfig *config) {
    /* Walkman styled setup walkthrough with dynamic QR code */
    vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA8(22, 26, 34, 255));

    draw_beveled_box(40, 20, 880, 504, RGBA8(26, 30, 40, 255), RGBA8(85, 96, 118, 255), RGBA8(14, 16, 22, 255));

    if (s_font) {
        vita2d_pgf_draw_text(s_font, 70, 56, RGBA8(30, 215, 96, 255), 1.25f, "PSVITAMAN - SPOTIFY SETUP & PAIRING");
        vita2d_pgf_draw_text(s_font, 70, 80, RGBA8(150, 160, 175, 255), 0.75f, "Scan the QR code with your phone camera to pair your Spotify account");
        vita2d_draw_line(60, 92, 900, 92, RGBA8(85, 96, 118, 255));
    }

    /* Format QR Code Target URL */
    char qr_target_url[512] = {0};
    char vita_ip[32] = {0};
    bool has_ip = http_server_get_local_ip(vita_ip, sizeof(vita_ip));

    const char *cid = (strlen(config->client_id) > 0 && strstr(config->client_id, "YOUR_") == NULL)
                      ? config->client_id
                      : DEFAULT_SPOTIFY_CLIENT_ID;

    if (strlen(cid) > 0 && strstr(cid, "YOUR_") == NULL) {
        snprintf(qr_target_url, sizeof(qr_target_url),
                 "https://biodam.github.io/psvitaman/?ip=%s&client_id=%s",
                 has_ip ? vita_ip : "127.0.0.1", cid);
    } else {
        snprintf(qr_target_url, sizeof(qr_target_url), "https://biodam.github.io/psvitaman/");
    }

    /* Left Column: QR Code Card */
    float qx = 65, qy = 108, qw = 315, qh = 390;
    draw_beveled_box(qx, qy, qw, qh, RGBA8(32, 38, 50, 255), RGBA8(85, 96, 118, 255), RGBA8(14, 16, 22, 255));

    if (s_font) {
        vita2d_pgf_draw_text(s_font, (int)(qx + 48), (int)(qy + 30), RGBA8(30, 215, 96, 255), 0.90f, "SCAN TO PAIR PHONE");
    }

    /* Render on-screen QR Code */
    draw_qr_code(qx + 52, qy + 48, qr_target_url, 210);

    if (s_font) {
        const char *badge = "Direct Phone OAuth Bridge";
        int bw = vita2d_pgf_text_width(s_font, 0.75f, badge);
        vita2d_pgf_draw_text(s_font, (int)(qx + (qw - bw) / 2), (int)(qy + 295), RGBA8(245, 170, 45, 255), 0.75f, badge);
        vita2d_pgf_draw_text(s_font, (int)(qx + 35), (int)(qy + 325), RGBA8(240, 242, 245, 255), 0.72f, "Point phone camera at QR code");
        vita2d_pgf_draw_text(s_font, (int)(qx + 30), (int)(qy + 350), RGBA8(150, 160, 175, 255), 0.70f, "Tap the notification to open link");
        vita2d_pgf_draw_text(s_font, (int)(qx + 40), (int)(qy + 375), RGBA8(30, 215, 96, 255), 0.70f, "No PC or file copy needed!");
    }

    /* Right Column: Setup Instructions Card */
    float rx = 395, ry = 108, rw = 505, rh = 390;
    draw_beveled_box(rx, ry, rw, rh, RGBA8(32, 38, 50, 255), RGBA8(85, 96, 118, 255), RGBA8(14, 16, 22, 255));

    if (s_font) {
        vita2d_pgf_draw_text(s_font, (int)(rx + 25), (int)(ry + 32), RGBA8(30, 215, 96, 255), 1.0f, "HOW IT WORKS (100% PHONE-ONLY)");

        vita2d_pgf_draw_text(s_font, (int)(rx + 25), (int)(ry + 70), RGBA8(240, 242, 245, 255), 0.85f,
                             "1. Ensure your phone is on the same Wi-Fi as PS Vita.");
        vita2d_pgf_draw_text(s_font, (int)(rx + 25), (int)(ry + 105), RGBA8(240, 242, 245, 255), 0.85f,
                             "2. Scan the QR code to open Spotify Authorization.");
        vita2d_pgf_draw_text(s_font, (int)(rx + 25), (int)(ry + 140), RGBA8(240, 242, 245, 255), 0.85f,
                             "3. Log in & tap 'Agree' -> your phone sends the token.");
        vita2d_pgf_draw_text(s_font, (int)(rx + 25), (int)(ry + 175), RGBA8(240, 242, 245, 255), 0.85f,
                             "4. PSVitaman detects the token and starts playback!");

        /* Status Mini-Panel */
        draw_beveled_box(rx + 20, ry + 215, rw - 40, 110, RGBA8(20, 24, 32, 255), RGBA8(14, 16, 22, 255), RGBA8(85, 96, 118, 255));
        vita2d_pgf_draw_text(s_font, (int)(rx + 35), (int)(ry + 242), RGBA8(30, 215, 96, 255), 0.85f, "[+] PAIRING SERVER: Active on port 8888");

        char ip_label[128];
        if (has_ip) {
            snprintf(ip_label, sizeof(ip_label), "Vita IP: http://%s:8888", vita_ip);
            vita2d_pgf_draw_text(s_font, (int)(rx + 35), (int)(ry + 270), RGBA8(240, 242, 245, 255), 0.82f, ip_label);
            vita2d_pgf_draw_text(s_font, (int)(rx + 35), (int)(ry + 298), RGBA8(150, 160, 175, 255), 0.72f, "Waiting for phone connection...");
        } else {
            vita2d_pgf_draw_text(s_font, (int)(rx + 35), (int)(ry + 270), RGBA8(225, 55, 45, 255), 0.82f, "Wi-Fi Disconnected!");
            vita2d_pgf_draw_text(s_font, (int)(rx + 35), (int)(ry + 298), RGBA8(150, 160, 175, 255), 0.72f, "Please connect to Wi-Fi in Vita Settings");
        }

        vita2d_pgf_draw_text(s_font, (int)(rx + 25), (int)(ry + 368), RGBA8(30, 215, 96, 255), 0.85f,
                             "Press [START] on Vita to reload config manually if needed.");
    }
}

static void render_qr_overlay(const AppConfig *config) {
    /* Semi-transparent dark backdrop overlay */
    vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA8(12, 15, 22, 225));

    /* Centered Modal Card */
    float mx = 230, my = 30, mw = 500, mh = 484;
    draw_beveled_box(mx, my, mw, mh, RGBA8(28, 34, 46, 255), RGBA8(30, 215, 96, 255), RGBA8(14, 16, 22, 255));

    if (s_font) {
        vita2d_pgf_draw_text(s_font, (int)(mx + 115), (int)(my + 38), RGBA8(30, 215, 96, 255), 1.15f, "PHONE PAIRING & CONNECT");
        vita2d_draw_line(mx + 20, my + 52, mx + mw - 20, my + 52, RGBA8(85, 96, 118, 255));
    }

    char qr_url[512] = {0};
    char vita_ip[32] = {0};
    bool has_ip = http_server_get_local_ip(vita_ip, sizeof(vita_ip));

    const char *overlay_cid = (strlen(config->client_id) > 0 && strstr(config->client_id, "YOUR_") == NULL)
                              ? config->client_id
                              : DEFAULT_SPOTIFY_CLIENT_ID;

    if (strlen(overlay_cid) > 0 && strstr(overlay_cid, "YOUR_") == NULL) {
        snprintf(qr_url, sizeof(qr_url),
                 "https://biodam.github.io/psvitaman/?ip=%s&client_id=%s",
                 has_ip ? vita_ip : "127.0.0.1", overlay_cid);
    } else {
        snprintf(qr_url, sizeof(qr_url), "https://biodam.github.io/psvitaman/");
    }

    /* Draw Centered QR Code */
    draw_qr_code(mx + 140, my + 72, qr_url, 220);

    if (s_font) {
        vita2d_pgf_draw_text(s_font, (int)(mx + 60), (int)(my + 345), RGBA8(240, 242, 245, 255), 0.85f,
                             "Scan with your phone to pair Spotify account");
        vita2d_pgf_draw_text(s_font, (int)(mx + 90), (int)(my + 375), RGBA8(245, 170, 45, 255), 0.80f,
                             "Target: PSVitaman Web Gateway");

        draw_beveled_box(mx + 40, my + 410, mw - 80, 48, RGBA8(20, 24, 32, 255), RGBA8(14, 16, 22, 255), RGBA8(85, 96, 118, 255));
        vita2d_pgf_draw_text(s_font, (int)(mx + 70), (int)(my + 440), RGBA8(30, 215, 96, 255), 0.85f,
                             "Press [SELECT] or [O] to return to deck");
    }
}

static void draw_multiline_text(vita2d_pgf *font, int x, int y, unsigned int color, float scale, int line_spacing, const char *text) {
    if (!font || !text) return;
    char buf[256];
    const char *p = text;
    int cur_y = y;
    while (*p) {
        int i = 0;
        while (*p && *p != '\n' && i < (int)sizeof(buf) - 1) {
            buf[i++] = *p++;
        }
        buf[i] = '\0';
        if (*p == '\n') p++;
        vita2d_pgf_draw_text(font, x, cur_y, color, scale, buf);
        cur_y += line_spacing;
    }
}

static void render_error_modal(const AppError *error) {
    if (!error || !error->is_active) return;

    /* Semi-transparent dark backdrop overlay */
    vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA8(10, 12, 18, 230));

    float mx = 180, my = 100, mw = 600, mh = 344;
    draw_beveled_box(mx, my, mw, mh, RGBA8(26, 30, 42, 255), RGBA8(225, 55, 45, 255), RGBA8(14, 16, 22, 255));

    /* Red Title Bar Header */
    vita2d_draw_rectangle(mx + 2, my + 2, mw - 4, 46, RGBA8(225, 55, 45, 255));
    if (s_font) {
        char title_buf[128];
        snprintf(title_buf, sizeof(title_buf), "[!] SYSTEM ALERT - %s (0x%04X)",
                 error->title, (unsigned int)error->code);
        vita2d_pgf_draw_text(s_font, (int)(mx + 20), (int)(my + 32), RGBA8(240, 242, 245, 255), 0.95f, title_buf);
    }

    /* Inner Details Container */
    draw_beveled_box(mx + 20, my + 64, mw - 40, 190, RGBA8(18, 22, 30, 255), RGBA8(14, 16, 22, 255), RGBA8(85, 96, 118, 255));

    if (s_font) {
        /* Section Tag */
        vita2d_pgf_draw_text(s_font, (int)(mx + 38), (int)(my + 95), RGBA8(245, 170, 45, 255), 0.82f, "Diagnostic Information:");

        /* Error Description Text (multi-line supported) */
        draw_multiline_text(s_font, (int)(mx + 38), (int)(my + 130), RGBA8(240, 242, 245, 255), 0.85f, 26, error->message);

        /* Action Advice Inset Bar */
        draw_beveled_box(mx + 20, my + 270, mw - 40, 52, RGBA8(14, 18, 24, 255), RGBA8(14, 16, 22, 255), RGBA8(30, 215, 96, 255));
        const char *hint = (strlen(error->action_hint) > 0) ? error->action_hint : "Press [X] or [O] to dismiss";
        vita2d_pgf_draw_text(s_font, (int)(mx + 38), (int)(my + 304), RGBA8(30, 215, 96, 255), 0.88f, hint);
    }
}

void ui_render(const SpotifyPlaybackState *state, int interpolated_progress_ms,
              const InputState *input, const AppConfig *config, bool is_syncing,
              bool show_qr_overlay, const AppError *error) {
    vita2d_start_drawing();
    vita2d_clear_screen();

    if (!config || !config->is_valid) {
        render_setup_guide(config);
    } else {
        /* Sync active theme with config */
        int active_theme_idx = config->theme;
        if (active_theme_idx < 0 || active_theme_idx >= THEME_COUNT) {
            active_theme_idx = s_current_theme;
        }
        const AppTheme *theme = &s_themes[active_theme_idx];

        /* Clear full background with theme chassis color */
        vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, theme->bg_chassis);

        /* Render Top Information HUD */
        render_top_hud(state, theme, s_shift_x, s_shift_y, is_syncing);

        /* Render Cassette Bay (Center) */
        render_cassette_bay(state, interpolated_progress_ms, theme, s_shift_x, s_shift_y);

        /* Render Bottom Transport Buttons (Walkman Mechanical Layout) */
        render_transport_bar(state, input, theme, s_shift_x, s_shift_y);

        if (show_qr_overlay) {
            render_qr_overlay(config);
        }
    }

    /* Modal error overlay draws on top of all screens if active */
    if (error && error->is_active) {
        render_error_modal(error);
    }

    vita2d_end_drawing();
    vita2d_swap_buffers();
}
