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
#include <psp2/power.h>
#include <psp2/motion.h>
#else
/* Fallback mocks for non-Vita compilers */
typedef void* vita2d_pgf;
#define RGBA8(r,g,b,a) ((((a)&0xFF)<<24)|(((b)&0xFF)<<16)|(((g)&0xFF)<<8)|((r)&0xFF))
static void vita2d_start_drawing(void) {}
static void vita2d_clear_screen(void) {}
static void vita2d_draw_rectangle(float x, float y, float w, float h, unsigned int c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
static void vita2d_draw_fill_circle(float x, float y, float r, unsigned int c) { (void)x;(void)y;(void)r;(void)c; }
static void vita2d_draw_line(float x0, float y0, float x1, float y1, unsigned int c) { (void)x0;(void)y0;(void)x1;(void)y1;(void)c; }
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
    unsigned int lcd_backlight;
    unsigned int lcd_text;
    unsigned int lcd_text_dim;
} AppTheme;

static const AppTheme s_themes[THEME_COUNT] = {
    [THEME_TPS_L2] = {
        .name = "1979 TPS-L2 Blue",
        .model_stamp = "PSVITAMAN - TPS-L2 STEREO",
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
        .lcd_backlight = RGBA8(65, 175, 255, 255),
        .lcd_text = RGBA8(10, 28, 54, 255),
        .lcd_text_dim = RGBA8(38, 105, 168, 255),
    },
    [THEME_SPORTS_YELLOW] = {
        .name = "1983 WM-F5 Sports",
        .model_stamp = "PSVITAMAN - SPORTS F5 ACTION",
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
        .lcd_backlight = RGBA8(255, 140, 24, 255),
        .lcd_text = RGBA8(42, 14, 0, 255),
        .lcd_text_dim = RGBA8(165, 75, 10, 255),
    },
    [THEME_GRAPHITE_DD] = {
        .name = "1982 WM-DD Graphite",
        .model_stamp = "PSVITAMAN - DIRECT DRIVE DD",
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
        .lcd_backlight = RGBA8(160, 210, 230, 255),
        .lcd_text = RGBA8(18, 34, 45, 255),
        .lcd_text_dim = RGBA8(80, 120, 140, 255),
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
        .lcd_backlight = RGBA8(0, 230, 118, 255),
        .lcd_text = RGBA8(0, 36, 12, 255),
        .lcd_text_dim = RGBA8(0, 120, 50, 255),
    },
    [THEME_WM2_RED] = {
        .name = "1981 WM-2 Red",
        .model_stamp = "PSVITAMAN - MODEL 2 STEREO",
        .bg_chassis = RGBA8(195, 32, 38, 255),
        .border_light = RGBA8(235, 75, 80, 255),
        .border_dark = RGBA8(110, 16, 22, 255),
        .cassette_shell = RGBA8(28, 30, 36, 255),
        .cassette_inner = RGBA8(16, 18, 22, 255),
        .label_bg = RGBA8(242, 240, 232, 255),
        .label_stripe_a = RGBA8(195, 32, 38, 255),
        .label_stripe_b = RGBA8(160, 165, 175, 255),
        .label_text = RGBA8(24, 25, 30, 255),
        .btn_normal = RGBA8(36, 38, 44, 255),
        .btn_pressed = RGBA8(18, 20, 24, 255),
        .btn_border_hi = RGBA8(75, 82, 95, 255),
        .btn_border_lo = RGBA8(12, 14, 16, 255),
        .btn_active_led = RGBA8(30, 215, 96, 255),
        .btn_theme_special = RGBA8(245, 175, 25, 255),
        .btn_theme_special_dark = RGBA8(160, 105, 12, 255),
        .text_primary = RGBA8(250, 250, 252, 255),
        .text_muted = RGBA8(240, 185, 190, 255),
        .text_accent = RGBA8(255, 215, 65, 255),
        .spool_hub = RGBA8(245, 245, 245, 255),
        .spool_gear = RGBA8(165, 170, 180, 255),
        .tape_brown = RGBA8(58, 40, 28, 255),
        .lcd_backlight = RGBA8(255, 165, 38, 255),
        .lcd_text = RGBA8(48, 18, 0, 255),
        .lcd_text_dim = RGBA8(175, 95, 15, 255),
    },
    [THEME_WM_D6C_PRO] = {
        .name = "1984 WM-D6C Pro",
        .model_stamp = "PSVITAMAN - PRO STUDIO D6C",
        .bg_chassis = RGBA8(18, 20, 22, 255),
        .border_light = RGBA8(115, 125, 138, 255),
        .border_dark = RGBA8(8, 9, 10, 255),
        .cassette_shell = RGBA8(14, 16, 20, 255),
        .cassette_inner = RGBA8(6, 8, 10, 255),
        .label_bg = RGBA8(32, 35, 42, 255),
        .label_stripe_a = RGBA8(218, 175, 55, 255),
        .label_stripe_b = RGBA8(200, 35, 35, 255),
        .label_text = RGBA8(225, 185, 65, 255),
        .btn_normal = RGBA8(28, 30, 36, 255),
        .btn_pressed = RGBA8(14, 15, 18, 255),
        .btn_border_hi = RGBA8(80, 88, 100, 255),
        .btn_border_lo = RGBA8(8, 9, 10, 255),
        .btn_active_led = RGBA8(245, 35, 35, 255),
        .btn_theme_special = RGBA8(195, 25, 35, 255),
        .btn_theme_special_dark = RGBA8(115, 12, 18, 255),
        .text_primary = RGBA8(245, 245, 240, 255),
        .text_muted = RGBA8(135, 145, 158, 255),
        .text_accent = RGBA8(225, 185, 65, 255),
        .spool_hub = RGBA8(205, 212, 220, 255),
        .spool_gear = RGBA8(88, 96, 110, 255),
        .tape_brown = RGBA8(38, 26, 18, 255),
        .lcd_backlight = RGBA8(255, 95, 30, 255),
        .lcd_text = RGBA8(45, 8, 4, 255),
        .lcd_text_dim = RGBA8(165, 45, 15, 255),
    },
    [THEME_POP_RED] = {
        .name = "1987 Pop Retro Red",
        .model_stamp = "PSVITAMAN - RETRO PLAYER",
        .bg_chassis = RGBA8(215, 35, 30, 255),
        .border_light = RGBA8(250, 80, 70, 255),
        .border_dark = RGBA8(125, 16, 12, 255),
        .cassette_shell = RGBA8(26, 28, 34, 255),
        .cassette_inner = RGBA8(14, 16, 20, 255),
        .label_bg = RGBA8(252, 252, 252, 255),
        .label_stripe_a = RGBA8(25, 145, 235, 255),
        .label_stripe_b = RGBA8(250, 200, 20, 255),
        .label_text = RGBA8(18, 28, 48, 255),
        .btn_normal = RGBA8(24, 48, 96, 255),
        .btn_pressed = RGBA8(12, 26, 56, 255),
        .btn_border_hi = RGBA8(55, 95, 175, 255),
        .btn_border_lo = RGBA8(10, 20, 42, 255),
        .btn_active_led = RGBA8(250, 205, 25, 255),
        .btn_theme_special = RGBA8(250, 200, 20, 255),
        .btn_theme_special_dark = RGBA8(170, 130, 10, 255),
        .text_primary = RGBA8(255, 255, 255, 255),
        .text_muted = RGBA8(255, 205, 200, 255),
        .text_accent = RGBA8(250, 205, 25, 255),
        .spool_hub = RGBA8(25, 145, 235, 255),
        .spool_gear = RGBA8(250, 200, 20, 255),
        .tape_brown = RGBA8(55, 38, 26, 255),
        .lcd_backlight = RGBA8(55, 195, 255, 255),
        .lcd_text = RGBA8(8, 34, 62, 255),
        .lcd_text_dim = RGBA8(32, 118, 185, 255),
    },
    [THEME_CHAMPAGNE_GOLD] = {
        .name = "1989 WM-701C Gold",
        .model_stamp = "PSVITAMAN - 10TH ANNIVERSARY",
        .bg_chassis = RGBA8(185, 165, 125, 255),
        .border_light = RGBA8(235, 220, 180, 255),
        .border_dark = RGBA8(95, 82, 58, 255),
        .cassette_shell = RGBA8(26, 24, 22, 255),
        .cassette_inner = RGBA8(15, 14, 12, 255),
        .label_bg = RGBA8(248, 245, 236, 255),
        .label_stripe_a = RGBA8(24, 38, 68, 255),
        .label_stripe_b = RGBA8(210, 175, 80, 255),
        .label_text = RGBA8(45, 38, 25, 255),
        .btn_normal = RGBA8(28, 34, 46, 255),
        .btn_pressed = RGBA8(14, 18, 26, 255),
        .btn_border_hi = RGBA8(60, 75, 102, 255),
        .btn_border_lo = RGBA8(10, 12, 18, 255),
        .btn_active_led = RGBA8(255, 180, 30, 255),
        .btn_theme_special = RGBA8(215, 180, 85, 255),
        .btn_theme_special_dark = RGBA8(145, 115, 45, 255),
        .text_primary = RGBA8(245, 240, 230, 255),
        .text_muted = RGBA8(155, 165, 180, 255),
        .text_accent = RGBA8(235, 195, 85, 255),
        .spool_hub = RGBA8(242, 238, 225, 255),
        .spool_gear = RGBA8(195, 168, 95, 255),
        .tape_brown = RGBA8(50, 36, 24, 255),
        .lcd_backlight = RGBA8(235, 205, 135, 255),
        .lcd_text = RGBA8(42, 34, 12, 255),
        .lcd_text_dim = RGBA8(155, 128, 68, 255),
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

/* Acrylic Glass Specular Reflection / Glare Animation State */
static float s_glare_timer = 0.0f;
static float s_next_glare_interval = 45.0f;
static float s_glare_progress = -0.5f;
static bool s_glare_active = false;
static float s_glare_cooldown = 0.0f;
static float s_prev_accel_x = 0.0f;
static float s_prev_accel_y = 0.0f;
static float s_prev_accel_z = 0.0f;
static float s_gyro_glare_x = 0.5f;
static float s_gyro_glare_tilt = 90.0f;
static bool s_motion_initialized = false;

bool ui_init(void) {
    s_font = vita2d_load_default_pgf();
#if defined(__psp2__) || defined(__VITA__)
    if (sceMotionStartSampling() >= 0) {
        s_motion_initialized = true;
    }
#endif
    return (s_font != NULL);
}

void ui_cleanup(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (s_motion_initialized) {
        sceMotionStopSampling();
        s_motion_initialized = false;
    }
#endif
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

    /* 7. Glass Glare / Reflection System Update */
    if (s_glare_cooldown > 0.0f) {
        s_glare_cooldown -= delta_time;
    }

#if defined(__psp2__) || defined(__VITA__)
    if (s_motion_initialized) {
        SceMotionState mstate;
        if (sceMotionGetState(&mstate) >= 0) {
            /* 7a. Device tilt directly drives dynamic reflection position on the acrylic window */
            float target_x = 0.5f + (mstate.acceleration.x * 0.50f);
            if (target_x < 0.12f) target_x = 0.12f;
            if (target_x > 0.88f) target_x = 0.88f;

            s_gyro_glare_x += (target_x - s_gyro_glare_x) * 5.0f * delta_time;

            float target_tilt = 90.0f + (mstate.acceleration.y * 25.0f);
            s_gyro_glare_tilt += (target_tilt - s_gyro_glare_tilt) * 4.0f * delta_time;

            /* 7b. Sudden vigorous motion triggers a graceful light sweep */
            if (s_glare_cooldown <= 0.0f) {
                float mag_gyro = fabsf(mstate.angularVelocity.x) + fabsf(mstate.angularVelocity.y) + fabsf(mstate.angularVelocity.z);
                float diff_accel = fabsf(mstate.acceleration.x - s_prev_accel_x) +
                                   fabsf(mstate.acceleration.y - s_prev_accel_y) +
                                   fabsf(mstate.acceleration.z - s_prev_accel_z);
                if (mag_gyro > 0.85f || diff_accel > 0.65f) {
                    s_glare_active = true;
                    s_glare_progress = -0.40f;
                    s_glare_cooldown = 8.0f; /* Debounce so continuous motion doesn't spam */
                }
            }
            s_prev_accel_x = mstate.acceleration.x;
            s_prev_accel_y = mstate.acceleration.y;
            s_prev_accel_z = mstate.acceleration.z;
        }
    }
#endif

    /* Ambient reflection occurs occasionally on random timer (45..85s) */
    s_glare_timer += delta_time;
    if (s_glare_timer >= s_next_glare_interval) {
        s_glare_timer = 0.0f;
        s_next_glare_interval = 45.0f + ((float)(rand() % 400) / 10.0f);
        if (!s_glare_active) {
            s_glare_active = true;
            s_glare_progress = -0.40f;
        }
    }

    if (s_glare_active) {
        s_glare_progress += 1.35f * delta_time; /* ~1.3s smooth sweep across window */
        if (s_glare_progress > 1.40f) {
            s_glare_active = false;
        }
    }
}

static inline unsigned int color_lerp(unsigned int c1, unsigned int c2, float t) {
    int r1 = (c1 >> 0) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = (c1 >> 16) & 0xFF, a1 = (c1 >> 24) & 0xFF;
    int r2 = (c2 >> 0) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = (c2 >> 16) & 0xFF, a2 = (c2 >> 24) & 0xFF;
    int r = r1 + (int)((r2 - r1) * t);
    int g = g1 + (int)((g2 - g1) * t);
    int b = b1 + (int)((b2 - b1) * t);
    int a = a1 + (int)((a2 - a1) * t);
    if (r > 255) { r = 255; } else if (r < 0) { r = 0; }
    if (g > 255) { g = 255; } else if (g < 0) { g = 0; }
    if (b > 255) { b = 255; } else if (b < 0) { b = 0; }
    return RGBA8((unsigned int)r, (unsigned int)g, (unsigned int)b, (unsigned int)a);
}

static inline unsigned int color_tint(unsigned int c, float factor) {
    int r = (int)(((c >> 0) & 0xFF) * factor);
    int g = (int)(((c >> 8) & 0xFF) * factor);
    int b = (int)(((c >> 16) & 0xFF) * factor);
    int a = (c >> 24) & 0xFF;
    if (r > 255) { r = 255; } else if (r < 0) { r = 0; }
    if (g > 255) { g = 255; } else if (g < 0) { g = 0; }
    if (b > 255) { b = 255; } else if (b < 0) { b = 0; }
    return RGBA8((unsigned int)r, (unsigned int)g, (unsigned int)b, (unsigned int)a);
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

static void draw_skeuomorphic_panel(float x, float y, float w, float h,
                                   unsigned int col_base, bool sunken) {
    /* 1. Ambient drop shadow for raised panel, or outer rim highlight for sunken cavity */
    if (!sunken) {
        vita2d_draw_rectangle(x + 2, y + h, w - 2, 3.0f, RGBA8(0, 0, 0, 45));
        vita2d_draw_rectangle(x + 4, y + h + 3, w - 6, 2.0f, RGBA8(0, 0, 0, 20));
        vita2d_draw_rectangle(x + w, y + 2, 3.0f, h - 2, RGBA8(0, 0, 0, 35));
    }

    /* 2. Soft directional surface gradient (light from above) */
    unsigned int col_top = sunken ? color_tint(col_base, 0.86f) : color_tint(col_base, 1.14f);
    unsigned int col_bot = sunken ? color_tint(col_base, 1.06f) : color_tint(col_base, 0.86f);
    float step = 2.0f;
    int num_steps = (int)(h / step);
    for (int i = 0; i < num_steps; i++) {
        float t = (float)i / (float)num_steps;
        vita2d_draw_rectangle(x, y + i * step, w, step, color_lerp(col_top, col_bot, t));
    }

    /* 3. Beveled chamfer light & shadow edges */
    unsigned int hi = sunken ? RGBA8(0, 0, 0, 100) : color_tint(col_base, 1.40f);
    unsigned int lo = sunken ? color_tint(col_base, 1.25f) : RGBA8(0, 0, 0, 115);

    vita2d_draw_line(x, y, x + w, y, hi);
    vita2d_draw_line(x + 1, y + 1, x + w - 1, y + 1, sunken ? RGBA8(0, 0, 0, 50) : color_tint(col_base, 1.22f));
    vita2d_draw_line(x, y, x, y + h, hi);
    vita2d_draw_line(x + 1, y + 1, x + 1, y + h - 1, sunken ? RGBA8(0, 0, 0, 50) : color_tint(col_base, 1.22f));

    vita2d_draw_line(x, y + h - 1, x + w, y + h - 1, lo);
    vita2d_draw_line(x + 1, y + h - 2, x + w - 1, y + h - 2, sunken ? color_tint(col_base, 1.15f) : RGBA8(0, 0, 0, 60));
    vita2d_draw_line(x + w - 1, y, x + w - 1, y + h, lo);
    vita2d_draw_line(x + w - 2, y + 1, x + w - 2, y + h - 1, sunken ? color_tint(col_base, 1.15f) : RGBA8(0, 0, 0, 60));
}

static void draw_lcd_note_icon(float cx, float cy, unsigned int color) {
    /* Procedural ♫ double eighth note glyph in liquid crystal ink */
    vita2d_draw_fill_circle(cx - 3.5f, cy + 3.0f, 2.0f, color);
    vita2d_draw_fill_circle(cx + 3.0f, cy + 1.5f, 2.0f, color);
    vita2d_draw_rectangle(cx - 2.5f, cy - 4.5f, 1.5f, 7.5f, color);
    vita2d_draw_rectangle(cx + 4.0f, cy - 6.0f, 1.5f, 7.5f, color);
    vita2d_draw_line(cx - 2.5f, cy - 4.5f, cx + 5.0f, cy - 6.0f, color);
    vita2d_draw_line(cx - 2.5f, cy - 3.5f, cx + 5.0f, cy - 5.0f, color);
}

static void draw_lcd_artist_icon(float cx, float cy, unsigned int color) {
    /* Procedural 👤 artist bust glyph in liquid crystal ink */
    vita2d_draw_fill_circle(cx, cy - 2.5f, 2.0f, color);
    vita2d_draw_rectangle(cx - 3.5f, cy + 1.0f, 7.0f, 3.0f, color);
}

static void draw_retro_lcd_screen(float x, float y, float w, float h,
                                  const SpotifyPlaybackState *state,
                                  int interpolated_progress_ms,
                                  const AppTheme *theme) {
    /* 1. Ambient Backlight Chassis Bleed Glow (Screen illuminates the dark housing) */
    vita2d_draw_rectangle(x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f, (theme->lcd_backlight & 0x00FFFFFF) | 0x14000000);
    vita2d_draw_rectangle(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, (theme->lcd_backlight & 0x00FFFFFF) | 0x22000000);

    /* 2. Deep sunken molded bezel frame with directional shadow */
    draw_skeuomorphic_panel(x, y, w, h, RGBA8(16, 18, 24, 255), true);

    /* 3. Inset Backlit LCD Glass Panel */
    float gx = x + 5.0f;
    float gy = y + 5.0f;
    float gw = w - 10.0f;
    float gh = h - 10.0f;

    /* Fluorescent / CCFL edge-lit backlighting gradient (diffuse glow with brighter center) */
    unsigned int bg_top = color_tint(theme->lcd_backlight, 1.08f);
    unsigned int bg_bot = color_tint(theme->lcd_backlight, 0.92f);
    float step = 5.0f;
    int num_steps = (int)(gh / step);
    for (int i = 0; i < num_steps; i++) {
        float t = (float)i / (float)num_steps;
        vita2d_draw_rectangle(gx, gy + i * step, gw, step + 1.0f, color_lerp(bg_top, bg_bot, t));
    }

    /* 4. Horizontal STN LCD electrode scanline micro-raster */
    for (float scan_y = gy + 1; scan_y < gy + gh - 1; scan_y += 3.0f) {
        vita2d_draw_line(gx, scan_y, gx + gw, scan_y, RGBA8(0, 0, 0, 14));
    }

    /* 5. Inset Chassis Lip Cast Shadow onto Glass (Top & Left depth) */
    for (int s = 0; s < 5; s++) {
        float alpha = (1.0f - (float)s / 5.0f) * 95.0f;
        vita2d_draw_line(gx, gy + s, gx + gw, gy + s, RGBA8(0, 0, 0, (unsigned int)alpha));
    }
    for (int s = 0; s < 3; s++) {
        float alpha = (1.0f - (float)s / 3.0f) * 75.0f;
        vita2d_draw_line(gx + s, gy, gx + s, gy + gh, RGBA8(0, 0, 0, (unsigned int)alpha));
    }

    /* 6. Inset glass rim specular highlights & inner bevel */
    vita2d_draw_line(gx, gy, gx + gw, gy, RGBA8(255, 255, 255, 55));
    vita2d_draw_line(gx, gy, gx, gy + gh, RGBA8(255, 255, 255, 55));
    vita2d_draw_line(gx, gy + gh - 1, gx + gw, gy + gh - 1, RGBA8(0, 0, 0, 80));
    vita2d_draw_line(gx + gw - 1, gy, gx + gw - 1, gy + gh, RGBA8(0, 0, 0, 80));

    /* 7. Diagonal Specular Glass Reflection Sheen (Top-right corner lens flare, analytically bounded) */
    for (int off = -18; off <= 18; off += 2) {
        float dist = fabsf((float)off);
        float alpha = (1.0f - dist / 18.0f) * 28.0f;
        float x1 = gx + gw - 90.0f + off;
        float y1 = gy;
        float x2 = gx + gw + off;
        float y2 = gy + 90.0f;
        if (x2 > gx + gw - 1.0f) {
            float d = x2 - (gx + gw - 1.0f);
            x2 = gx + gw - 1.0f;
            y2 -= d;
        }
        if (y2 > gy + gh - 1.0f) {
            float d = y2 - (gy + gh - 1.0f);
            y2 = gy + gh - 1.0f;
            x2 -= d;
        }
        if (x1 < gx) {
            float d = gx - x1;
            x1 = gx;
            y1 += d;
        }
        if (x1 < x2 && y1 < y2) {
            vita2d_draw_line(x1, y1, x2, y2, RGBA8(255, 255, 255, (unsigned int)alpha));
        }
    }
    vita2d_draw_line(gx + gw - 90.0f, gy, gx + gw - 1.0f, gy + 89.0f, RGBA8(255, 255, 255, 60));

    if (!s_font) return;

    /* 8. Dynamic Information Layout on the LCD */
    unsigned int ink = theme->lcd_text;
    unsigned int ink_dim = theme->lcd_text_dim;
    unsigned int ghost_ink = (ink_dim & 0x00FFFFFF) | 0x2A000000;

    /* Row 1: Header Line (Y: gy + 17) */
    /* 1a. Left Playback Status Badge with procedural glyph */
    if (state->is_playing) {
        /* Vector Play Triangle */
        vita2d_draw_line(gx + 14, gy + 9, gx + 20, gy + 13, ink);
        vita2d_draw_line(gx + 20, gy + 13, gx + 14, gy + 17, ink);
        vita2d_draw_line(gx + 14, gy + 17, gx + 14, gy + 9, ink);
        vita2d_pgf_draw_text(s_font, (int)(gx + 24), (int)(gy + 17), ink, 0.70f, "PLAYING");
    } else if (state->duration_ms > 0) {
        /* Vector Pause Bars */
        vita2d_draw_rectangle(gx + 14, gy + 9, 2.5f, 8.0f, ink);
        vita2d_draw_rectangle(gx + 18, gy + 9, 2.5f, 8.0f, ink);
        vita2d_pgf_draw_text(s_font, (int)(gx + 24), (int)(gy + 17), ink, 0.70f, "PAUSED");
    } else {
        /* Vector Stop Square */
        vita2d_draw_rectangle(gx + 14, gy + 9, 7.0f, 7.0f, ink_dim);
        vita2d_pgf_draw_text(s_font, (int)(gx + 24), (int)(gy + 17), ink_dim, 0.70f, "STOPPED");
    }

    /* 1b. Center Segmented Progress Bar + Time Elapsed / Remaining */
    char cur_time[16], total_time[16];
    utils_format_time_ms(interpolated_progress_ms, cur_time, sizeof(cur_time));
    utils_format_time_ms(state->duration_ms, total_time, sizeof(total_time));

    float ratio = 0.0f;
    if (state->duration_ms > 0) {
        ratio = (float)interpolated_progress_ms / (float)state->duration_ms;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
    }

    const int num_segments = 24;
    const float seg_w = 9.0f;
    const float seg_gap = 2.0f;
    const float seg_h = 6.0f;
    float total_seg_w = num_segments * seg_w + (num_segments - 1) * seg_gap;
    int active_segs = (int)(ratio * num_segments);

    int tw_cur = vita2d_pgf_text_width(s_font, 0.68f, cur_time);
    int tw_tot = vita2d_pgf_text_width(s_font, 0.68f, total_time);
    float total_bar_area_w = (float)tw_cur + 8.0f + total_seg_w + 8.0f + (float)tw_tot;
    float bar_start_x = gx + (gw - total_bar_area_w) * 0.5f;

    /* Current elapsed time */
    vita2d_pgf_draw_text(s_font, (int)bar_start_x, (int)(gy + 17), ink, 0.68f, cur_time);

    /* Progress bar segments */
    float seg_x_origin = bar_start_x + tw_cur + 8.0f;
    float seg_y = gy + 11.0f;
    for (int s = 0; s < num_segments; s++) {
        float sx = seg_x_origin + s * (seg_w + seg_gap);
        if (s < active_segs) {
            vita2d_draw_rectangle(sx, seg_y, seg_w, seg_h, ink);
            vita2d_draw_line(sx, seg_y, sx + seg_w - 1.0f, seg_y, color_tint(theme->lcd_backlight, 1.25f));
        } else {
            vita2d_draw_rectangle(sx, seg_y, seg_w, seg_h, ghost_ink);
        }
    }

    /* Total track duration */
    vita2d_pgf_draw_text(s_font, (int)(seg_x_origin + total_seg_w + 8.0f), (int)(gy + 17), ink, 0.68f, total_time);

    /* 1c. Right Badges: Shuffle & Repeat with ghost segment templates */
    const char *rep_str = (state->repeat_state == REPEAT_TRACK) ? "[REP 1]" : ((state->repeat_state == REPEAT_CONTEXT) ? "[REP ALL]" : "[REP]");
    unsigned int rep_col = (state->repeat_state != REPEAT_OFF) ? ink : ghost_ink;
    int rw = vita2d_pgf_text_width(s_font, 0.66f, "[REP ALL]");
    int rep_x = (int)(gx + gw - rw - 14);
    vita2d_pgf_draw_text(s_font, rep_x, (int)(gy + 17), ghost_ink, 0.66f, "[REP ALL]");
    vita2d_pgf_draw_text(s_font, rep_x, (int)(gy + 17), rep_col, 0.66f, rep_str);

    const char *shuf_str = "[SHUF]";
    unsigned int shuf_col = state->shuffle_state ? ink : ghost_ink;
    int sw = vita2d_pgf_text_width(s_font, 0.66f, shuf_str);
    int shuf_x = rep_x - sw - 10;
    vita2d_pgf_draw_text(s_font, shuf_x, (int)(gy + 17), ghost_ink, 0.66f, shuf_str);
    vita2d_pgf_draw_text(s_font, shuf_x, (int)(gy + 17), shuf_col, 0.66f, shuf_str);

    /* Row 2: Procedural ♫ + Track Title Marquee (Y: gy + 42) */
    /* Procedural ♫ Music Note Glyph */
    draw_lcd_note_icon(gx + 18.0f, gy + 42.0f, ink);

    const char *track_title = (strlen(state->track_name) > 0) ? state->track_name : "Playback Idle / Stopped";
    char title_buf[600];
    snprintf(title_buf, sizeof(title_buf), "%s", track_title);

    float title_clip_x = gx + 28.0f;
    float title_clip_y = gy + 28.0f;
    float title_clip_w = gw - 42.0f;

    int title_w = vita2d_pgf_text_width(s_font, 1.05f, title_buf);
    if (title_w > (int)title_clip_w) {
        /* Retro character stepper across the matrix cells (classic Walkman LCD marquee) */
        int len = (int)strlen(title_buf);
        char display_buf[256];
        int scroll_chars = (int)(s_marquee_offset / 16.0f);
        int start_idx = scroll_chars % (len + 6);
        if (start_idx < len) {
            snprintf(display_buf, sizeof(display_buf), "%s   %s", title_buf + start_idx, title_buf);
        } else {
            snprintf(display_buf, sizeof(display_buf), "%s", title_buf);
        }
        int fit_len = (int)strlen(display_buf);
        while (fit_len > 0 && vita2d_pgf_text_width(s_font, 1.05f, display_buf) > (int)title_clip_w) {
            display_buf[--fit_len] = '\0';
        }
        vita2d_pgf_draw_text(s_font, (int)title_clip_x, (int)(title_clip_y + 18), ink, 1.05f, display_buf);
    } else {
        vita2d_pgf_draw_text(s_font, (int)title_clip_x, (int)(title_clip_y + 18), ink, 1.05f, title_buf);
    }

    /* Row 3: Subtitle on Left, Graphic Spectrum Analyzer on Right (Y: gy + 67) */
    draw_lcd_artist_icon(gx + 18.0f, gy + 64.0f, ink);

    char sub_buf[600] = {0};
    if (strlen(state->artist_name) > 0) {
        if (strlen(state->album_name) > 0) {
            snprintf(sub_buf, sizeof(sub_buf), "%s  •  %s", state->artist_name, state->album_name);
        } else {
            utils_safe_strncpy(sub_buf, state->artist_name, sizeof(sub_buf));
        }
    } else {
        utils_safe_strncpy(sub_buf, "Connect Spotify from Phone, PC, or Console", sizeof(sub_buf));
    }

    /* Row 3 (Right): Graphic Spectrum Analyzer (14-Band LCD Audio Bars) */
    const int num_bands = 14;
    const float bar_w = 8.0f;
    const float bar_gap = 3.0f;
    const int num_blocks = 6;
    const float block_h = 2.8f;
    const float block_gap = 1.2f;
    float total_spectrum_w = num_bands * bar_w + (num_bands - 1) * bar_gap;
    float spec_x = gx + gw - total_spectrum_w - 14.0f;
    float spec_base_y = gy + 74.0f;

    /* Left subtitle text clipped nicely before spectrum bars */
    float sub_clip_x = gx + 28.0f;
    float sub_clip_w = (spec_x - sub_clip_x) - 16.0f;
    int sub_len = (int)strlen(sub_buf);
    while (sub_len > 0 && vita2d_pgf_text_width(s_font, 0.72f, sub_buf) > (int)sub_clip_w) {
        sub_buf[--sub_len] = '\0';
    }
    vita2d_pgf_draw_text(s_font, (int)sub_clip_x, (int)(gy + 68), ink, 0.72f, sub_buf);

    /* Draw Spectrum Analyzer Bars */
    for (int b = 0; b < num_bands; b++) {
        float bx = spec_x + b * (bar_w + bar_gap);
        int lit_blocks = 0;

        if (state->is_playing) {
            float val = 0.0f;
            if (b < 4) {
                /* Bass / kick rhythm */
                val = fabsf(sinf(s_flutter_time * 0.32f + b * 0.5f)) * 0.65f +
                      fabsf(cosf(s_flutter_time * 0.68f + b * 0.3f)) * 0.45f;
            } else if (b < 10) {
                /* Mid / vocal harmonics */
                val = fabsf(sinf(s_flutter_time * 0.55f + b * 0.75f)) * 0.60f +
                      fabsf(cosf(s_flutter_time * 1.15f + b * 0.4f)) * 0.50f;
            } else {
                /* High / treble flutter */
                val = fabsf(sinf(s_flutter_time * 1.35f + b * 1.1f)) * 0.55f +
                      fabsf(sinf(s_flutter_time * 2.2f + b * 0.8f)) * 0.45f;
            }
            if (val > 1.0f) val = 1.0f;
            lit_blocks = 1 + (int)(val * 5.0f); /* 1..6 blocks */
        }

        for (int blk = 0; blk < num_blocks; blk++) {
            float blk_y = spec_base_y - (blk + 1) * (block_h + block_gap);
            if (blk < lit_blocks) {
                vita2d_draw_rectangle(bx, blk_y, bar_w, block_h, ink);
                vita2d_draw_line(bx, blk_y, bx + bar_w - 1.0f, blk_y, color_tint(theme->lcd_backlight, 1.20f));
            } else {
                vita2d_draw_rectangle(bx, blk_y, bar_w, block_h, ghost_ink);
            }
        }
    }
}

static void draw_skeuomorphic_key(float x, float y, float w, float h,
                                  unsigned int base_col, bool is_pressed, bool is_latched,
                                  unsigned int active_glow_col) {
    float y_off = 0.0f;
    float h_adj = 0.0f;
    if (is_pressed) {
        y_off = is_latched ? 7.0f : 4.0f;
        h_adj = is_latched ? -6.0f : -3.0f;
    } else if (is_latched) {
        y_off = 5.0f;
        h_adj = -4.0f;
    }

    float draw_y = y + y_off;
    float draw_h = h + h_adj;

    /* 1. Key Drop Shadow underneath inside the trench */
    if (!is_latched && !is_pressed) {
        /* High profile key shadow when unpressed / OFF */
        vita2d_draw_rectangle(x + 2, draw_y + draw_h, w - 4, 5.0f, RGBA8(0, 0, 0, 95));
        vita2d_draw_rectangle(x + 4, draw_y + draw_h + 5, w - 8, 3.0f, RGBA8(0, 0, 0, 40));
    } else {
        /* Recessed inside the slot - subtle contact shadow */
        vita2d_draw_rectangle(x + 1, draw_y + draw_h, w - 2, 2.0f, RGBA8(0, 0, 0, 120));
    }

    /* 2. Vertical surface gradient for 3D body */
    unsigned int col_top, col_bot;
    if (is_latched || is_pressed) {
        /* Deep sunken/recessed face (darker tone inside the trench) */
        col_top = color_tint(base_col, 0.80f);
        col_bot = color_tint(base_col, 0.68f);
    } else {
        /* Bright elevated key profile */
        col_top = color_tint(base_col, 1.22f);
        col_bot = color_tint(base_col, 0.86f);
    }

    float step = 2.0f;
    int num_steps = (int)(draw_h / step);
    for (int i = 0; i < num_steps; i++) {
        float t = (float)i / (float)num_steps;
        vita2d_draw_rectangle(x, draw_y + i * step, w, step, color_lerp(col_top, col_bot, t));
    }

    /* 3. Beveled chamfer edge highlights & contact shadows */
    unsigned int top_hi = (is_latched || is_pressed) ? color_tint(base_col, 0.95f) : color_tint(base_col, 1.45f);
    unsigned int bot_lo = (is_latched || is_pressed) ? color_tint(base_col, 0.58f) : RGBA8(0, 0, 0, 125);

    vita2d_draw_line(x, draw_y, x + w, draw_y, top_hi);
    vita2d_draw_line(x + 1, draw_y + 1, x + w - 1, draw_y + 1, (is_latched || is_pressed) ? RGBA8(0, 0, 0, 90) : color_tint(base_col, 1.25f));
    vita2d_draw_line(x, draw_y, x, draw_y + draw_h, top_hi);
    vita2d_draw_line(x + w - 1, draw_y, x + w - 1, draw_y + draw_h, bot_lo);
    vita2d_draw_line(x, draw_y + draw_h - 1, x + w, draw_y + draw_h - 1, bot_lo);

    /* 4. Trench Lip Overhang Shadow (When latched or pressed, slot lip casts deep shadow onto top of key) */
    if (is_latched || is_pressed) {
        for (int s = 0; s < 10; s++) {
            float alpha = (1.0f - (float)s / 10.0f) * 160.0f;
            vita2d_draw_rectangle(x, draw_y + s, w, 1.0f, RGBA8(0, 0, 0, (unsigned int)alpha));
        }
        /* Side shadow lines from adjacent elevated keys */
        vita2d_draw_line(x, draw_y, x, draw_y + draw_h, RGBA8(0, 0, 0, 120));
        vita2d_draw_line(x + w - 1, draw_y, x + w - 1, draw_y + draw_h, RGBA8(0, 0, 0, 120));
    }

    /* 5. Tactile Spherical Concave Finger-Cup (Centered on key body) */
    float cup_cx = x + w * 0.5f;
    float cup_cy = draw_y + draw_h * 0.48f;
    float cup_r = 27.0f;

    if (is_latched || is_pressed) {
        /* Illuminated ambient halo around the cup rim */
        vita2d_draw_fill_circle(cup_cx, cup_cy, cup_r + 2.5f, (active_glow_col & 0x00FFFFFF) | 0x22000000);
        vita2d_draw_fill_circle(cup_cx, cup_cy, cup_r + 1.0f, (active_glow_col & 0x00FFFFFF) | 0x42000000);

        /* Deep concave bowl with dark occlusion */
        vita2d_draw_fill_circle(cup_cx, cup_cy, cup_r, color_tint(base_col, 0.70f));
        vita2d_draw_fill_circle(cup_cx - 1.5f, cup_cy - 1.5f, cup_r * 0.94f, RGBA8(0, 0, 0, 85));
        vita2d_draw_fill_circle(cup_cx + 1.5f, cup_cy + 1.5f, cup_r * 0.88f, (active_glow_col & 0x00FFFFFF) | 0x28000000);
        vita2d_draw_fill_circle(cup_cx, cup_cy, cup_r * 0.72f, color_tint(base_col, 0.65f));
    } else {
        /* Unlatched / OFF concave bowl */
        vita2d_draw_fill_circle(cup_cx, cup_cy, cup_r, color_tint(base_col, 0.88f));
        vita2d_draw_fill_circle(cup_cx - 1.5f, cup_cy - 1.5f, cup_r * 0.94f, RGBA8(0, 0, 0, 45));
        vita2d_draw_fill_circle(cup_cx + 1.5f, cup_cy + 1.5f, cup_r * 0.88f, RGBA8(255, 255, 255, 34));
        vita2d_draw_fill_circle(cup_cx, cup_cy, cup_r * 0.72f, color_tint(base_col, 0.92f));
        vita2d_draw_fill_circle(cup_cx + 0.5f, cup_cy + 0.5f, cup_r * 0.48f, color_tint(base_col, 0.95f));
    }
}

static void draw_skeuomorphic_led(float cx, float cy, unsigned int led_color, bool is_lit) {
    /* 1. Deep counter-sunk mounting well */
    vita2d_draw_fill_circle(cx, cy, 6.0f, RGBA8(14, 16, 22, 255));
    vita2d_draw_fill_circle(cx, cy, 4.5f, RGBA8(8, 10, 14, 255));

    if (is_lit) {
        /* Multi-layer radiant bloom glow */
        vita2d_draw_fill_circle(cx, cy, 14.0f, (led_color & 0x00FFFFFF) | 0x18000000);
        vita2d_draw_fill_circle(cx, cy, 9.5f, (led_color & 0x00FFFFFF) | 0x38000000);
        vita2d_draw_fill_circle(cx, cy, 6.0f, (led_color & 0x00FFFFFF) | 0x65000000);

        /* Glowing LED bulb core */
        vita2d_draw_fill_circle(cx, cy, 3.8f, led_color);
        /* Specular bulb glint */
        vita2d_draw_fill_circle(cx - 1.0f, cy - 1.0f, 1.5f, RGBA8(255, 255, 255, 220));
    } else {
        /* Dark unlit acrylic bead with subtle specular catch */
        vita2d_draw_fill_circle(cx, cy, 3.5f, RGBA8(36, 42, 52, 255));
        vita2d_draw_fill_circle(cx - 1.0f, cy - 1.0f, 1.0f, RGBA8(140, 150, 165, 180));
    }
}

static void draw_acoustic_grille(float cx, float cy, float spacing) {
    /* 7-Hole Acoustic Perforation Matrix (Dieter Rams / Image 2 style) */
    const float offsets[7][2] = {
        { 0.0f, 0.0f },
        { 0.0f, -spacing },
        { 0.0f,  spacing },
        { -spacing * 0.866f, -spacing * 0.5f },
        { -spacing * 0.866f,  spacing * 0.5f },
        {  spacing * 0.866f, -spacing * 0.5f },
        {  spacing * 0.866f,  spacing * 0.5f },
    };

    for (int i = 0; i < 7; i++) {
        float hx = cx + offsets[i][0];
        float hy = cy + offsets[i][1];

        /* Dark hole interior */
        vita2d_draw_fill_circle(hx, hy, 2.5f, RGBA8(12, 14, 18, 255));
        /* Bottom specular rim reflection */
        vita2d_draw_line(hx - 2.0f, hy + 2.0f, hx + 2.0f, hy + 2.0f, RGBA8(255, 255, 255, 50));
    }
}

static void draw_rotary_volume_knob(float cx, float cy, float radius, int volume_percent, const AppTheme *theme) {
    /* 1. Recessed circular well with inner shadow */
    vita2d_draw_fill_circle(cx, cy, radius + 4.0f, RGBA8(14, 16, 22, 255));
    vita2d_draw_fill_circle(cx, cy, radius + 3.0f, RGBA8(22, 26, 34, 255));

    /* 2. Drop shadow under the raised turned metal knob */
    vita2d_draw_fill_circle(cx + 1.5f, cy + 3.0f, radius, RGBA8(0, 0, 0, 75));

    /* 3. Outer turned metallic bezel rim */
    vita2d_draw_fill_circle(cx, cy, radius, RGBA8(185, 192, 205, 255));
    vita2d_draw_fill_circle(cx, cy, radius - 1.5f, RGBA8(80, 86, 98, 255));

    /* 4. Cylindrical knob face with vertical gradient */
    unsigned int knob_top = RGBA8(68, 74, 88, 255);
    unsigned int knob_bot = RGBA8(38, 42, 52, 255);
    vita2d_draw_fill_circle(cx, cy, radius - 2.5f, knob_bot);
    vita2d_draw_fill_circle(cx, cy - 1.0f, radius - 3.5f, knob_top);
    vita2d_draw_fill_circle(cx, cy, radius - 4.5f, knob_bot);

    /* 5. Turned concentric micro-grooves */
    vita2d_draw_fill_circle(cx, cy, radius * 0.75f, RGBA8(55, 60, 72, 255));
    vita2d_draw_fill_circle(cx, cy, radius * 0.70f, RGBA8(44, 48, 58, 255));

    /* 6. Glowing Indicator Notch (0% -> -135 deg, 100% -> +135 deg) */
    float norm_vol = (float)volume_percent / 100.0f;
    if (norm_vol < 0.0f) norm_vol = 0.0f;
    if (norm_vol > 1.0f) norm_vol = 1.0f;

    float angle_deg = -135.0f + norm_vol * 270.0f;
    float rad = (angle_deg - 90.0f) * (3.14159265f / 180.0f);
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);

    float p1_x = cx + cos_a * (radius * 0.35f);
    float p1_y = cy + sin_a * (radius * 0.35f);
    float p2_x = cx + cos_a * (radius * 0.85f);
    float p2_y = cy + sin_a * (radius * 0.85f);

    /* Radiant illuminated notch */
    vita2d_draw_line(p1_x, p1_y, p2_x, p2_y, theme->btn_active_led);
    float px = -sin_a * 1.0f;
    float py = cos_a * 1.0f;
    vita2d_draw_line(p1_x + px, p1_y + py, p2_x + px, p2_y + py, theme->btn_active_led);
    vita2d_draw_line(p1_x - px, p1_y - py, p2_x - px, p2_y - py, (theme->btn_active_led & 0x00FFFFFF) | 0x88000000);

    /* Tiny bright tip dot */
    vita2d_draw_fill_circle(p2_x, p2_y, 1.5f, RGBA8(255, 255, 255, 230));
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

static void draw_screw(float x, float y) {
    /* Countersunk chassis screw with slot highlight */
    vita2d_draw_fill_circle(x, y + 1.0f, 6.5f, RGBA8(0, 0, 0, 80));
    vita2d_draw_fill_circle(x, y, 6.0f, RGBA8(180, 186, 196, 255));
    vita2d_draw_fill_circle(x, y, 4.5f, RGBA8(130, 136, 148, 255));
    vita2d_draw_line(x - 4, y, x + 4, y, RGBA8(50, 55, 65, 255));
    vita2d_draw_line(x - 3, y - 1, x + 3, y - 1, RGBA8(220, 225, 235, 180));
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
    float kw = 52.0f;
    float kh = 32.0f;

    /* 3D Odometer Frame with drop shadow */
    vita2d_draw_rectangle(kx + 1, ky + kh, kw, 2.0f, RGBA8(0, 0, 0, 75));
    draw_skeuomorphic_panel(kx, ky, kw, kh, RGBA8(14, 16, 22, 255), true);

    if (s_font) {
        /* "COUNTER" label above with increased legibility */
        vita2d_pgf_draw_text(s_font, (int)kx, (int)(ky - 6), theme->text_muted, 0.65f, "COUNTER");

        char c_buf[8];
        snprintf(c_buf, sizeof(c_buf), "%03d", counter_val % 1000);

        /* 3 rotary digit slots with cylindrical odometer bevels */
        for (int i = 0; i < 3; i++) {
            float wx = kx + 3.0f + i * 15.0f;
            /* Rotating wheel groove */
            vita2d_draw_rectangle(wx, ky + 4.0f, 14.0f, 24.0f, RGBA8(22, 25, 32, 255));
            vita2d_draw_line(wx, ky + 4.0f, wx + 13.0f, ky + 4.0f, RGBA8(8, 10, 14, 255)); /* top wheel shadow */
            vita2d_draw_line(wx, ky + 27.0f, wx + 13.0f, ky + 27.0f, RGBA8(8, 10, 14, 255)); /* bottom wheel shadow */
            vita2d_draw_line(wx, ky + 4.0f, wx, ky + 28.0f, RGBA8(40, 45, 55, 255));
            vita2d_draw_line(wx + 13.0f, ky + 4.0f, wx + 13.0f, ky + 28.0f, RGBA8(5, 7, 10, 255));

            char d_str[2] = { c_buf[i], '\0' };
            int dw = vita2d_pgf_text_width(s_font, 0.85f, d_str);
            vita2d_pgf_draw_text(s_font, (int)(wx + (14.0f - dw) * 0.5f), (int)(ky + 23.0f), RGBA8(240, 242, 245, 255), 0.85f, d_str);
        }

        /* 3D cylindrical reset push-button */
        float rx = kx + kw + 5.0f;
        float ry = ky + kh * 0.5f;
        vita2d_draw_fill_circle(rx + 0.5f, ry + 1.0f, 3.5f, RGBA8(0, 0, 0, 80));
        vita2d_draw_fill_circle(rx, ry, 3.0f, RGBA8(210, 215, 225, 255));
        vita2d_draw_fill_circle(rx - 0.5f, ry - 0.5f, 1.5f, RGBA8(255, 255, 255, 220));
    }
}

static void draw_vu_meters(float vx, float vy, bool is_playing, float anim_phase, const AppTheme *theme) {
    float vw = 50.0f;
    float vh = 100.0f;

    /* 3D Sunken VU well */
    draw_skeuomorphic_panel(vx, vy, vw, vh, RGBA8(12, 14, 18, 255), true);

    if (s_font) {
        vita2d_pgf_draw_text(s_font, (int)(vx + 8), (int)(vy + 14), theme->text_muted, 0.60f, "L");
        vita2d_pgf_draw_text(s_font, (int)(vx + 30), (int)(vy + 14), theme->text_muted, 0.60f, "R");
    }

    int lit_l = 0;
    int lit_r = 0;
    if (is_playing) {
        float s1 = fabsf(sinf(anim_phase * 0.12f));
        float s2 = fabsf(cosf(anim_phase * 0.17f + 0.5f));
        lit_l = 1 + (int)(s1 * 4.99f);
        lit_r = 1 + (int)(s2 * 4.99f);
    }

    /* 6 horizontal LED bars per channel with beveled bezels and glowing bloom */
    for (int i = 0; i < 6; i++) {
        float bar_y = vy + 81.0f - i * 11.5f;
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

        /* Left channel bar */
        bool left_lit = (i < lit_l);
        unsigned int cl = left_lit ? col_active : col_inactive;
        vita2d_draw_rectangle(vx + 6.0f, bar_y, 16.0f, 7.5f, cl);
        if (left_lit) {
            vita2d_draw_line(vx + 6.0f, bar_y, vx + 21.0f, bar_y, RGBA8(255, 255, 255, 130)); /* highlight */
        }

        /* Right channel bar */
        bool right_lit = (i < lit_r);
        unsigned int cr = right_lit ? col_active : col_inactive;
        vita2d_draw_rectangle(vx + 28.0f, bar_y, 16.0f, 7.5f, cr);
        if (right_lit) {
            vita2d_draw_line(vx + 28.0f, bar_y, vx + 43.0f, bar_y, RGBA8(255, 255, 255, 130)); /* highlight */
        }
    }
}

static void draw_battery_indicator(float bx, float by, const AppTheme *theme) {
    int bat_pct = 100;
    bool is_charging = false;
    bool is_low = false;

#if defined(__psp2__) || defined(__VITA__)
    bat_pct = scePowerGetBatteryLifePercent();
    is_charging = (scePowerIsBatteryCharging() == 1);
    is_low = (scePowerIsLowBattery() == 1);
#endif

    float bw = 28.0f;
    float bh = 14.0f;
    float x = bx - bw * 0.5f;
    float y = by - bh * 0.5f;

    /* 1. Outer brushed metallic battery casing with drop shadow */
    vita2d_draw_rectangle(x + 1, y + bh, bw, 2.0f, RGBA8(0, 0, 0, 50));
    vita2d_draw_rectangle(x, y, bw, bh, RGBA8(180, 186, 196, 255));
    vita2d_draw_line(x, y, x + bw, y, RGBA8(240, 245, 255, 255));
    vita2d_draw_line(x, y + bh - 1, x + bw, y + bh - 1, RGBA8(80, 85, 95, 255));

    /* 2. Positive Terminal Nub on Right */
    vita2d_draw_rectangle(x + bw, by - 3.0f, 3.0f, 6.0f, RGBA8(210, 215, 225, 255));
    vita2d_draw_line(x + bw, by - 3.0f, x + bw + 3.0f, by - 3.0f, RGBA8(255, 255, 255, 220));

    /* 3. Deep inner cavity */
    float inner_x = x + 2.0f;
    float inner_y = y + 2.0f;
    float inner_w = bw - 4.0f;
    float inner_h = bh - 4.0f;
    vita2d_draw_rectangle(inner_x, inner_y, inner_w, inner_h, RGBA8(14, 16, 22, 255));

    unsigned int fill_col = theme->btn_active_led;
    if (is_charging) {
        fill_col = RGBA8(255, 195, 25, 255);
    } else if (is_low || (bat_pct >= 0 && bat_pct <= 20)) {
        fill_col = RGBA8(235, 45, 45, 255);
    }

    if (bat_pct >= 0) {
        float ratio = (float)bat_pct / 100.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        if (ratio < 0.0f) ratio = 0.0f;

        float cur_w = inner_w * ratio;
        if (cur_w > 1.0f) {
            vita2d_draw_rectangle(inner_x, inner_y, cur_w, inner_h, fill_col);
            vita2d_draw_line(inner_x, inner_y + 1, inner_x + cur_w - 1, inner_y + 1, RGBA8(255, 255, 255, 90));
        }

        /* 3 Segments divider lines */
        for (int seg = 1; seg <= 2; seg++) {
            float div_x = inner_x + seg * (inner_w / 3.0f);
            vita2d_draw_line(div_x, inner_y, div_x, inner_y + inner_h, RGBA8(12, 14, 18, 190));
        }
    } else {
        vita2d_draw_rectangle(inner_x, inner_y, inner_w, inner_h, theme->btn_active_led);
    }

    /* 4. Text Display (% or AC) */
    if (s_font) {
        char bat_str[16];
        if (bat_pct >= 0) {
            if (is_charging) {
                snprintf(bat_str, sizeof(bat_str), "+%d%%", bat_pct);
            } else {
                snprintf(bat_str, sizeof(bat_str), "%d%%", bat_pct);
            }
        } else {
            snprintf(bat_str, sizeof(bat_str), "AC");
        }

        int tw = vita2d_pgf_text_width(s_font, 0.74f, bat_str);
        unsigned int text_col = (bat_pct >= 0 && bat_pct <= 20 && !is_charging)
            ? RGBA8(235, 60, 60, 255)
            : (is_charging ? RGBA8(255, 205, 50, 255) : theme->text_muted);

        vita2d_pgf_draw_text(s_font, (int)(x - tw - 8.0f), (int)(by + 6.0f), text_col, 0.74f, bat_str);
    }
}

static void render_top_hud(const SpotifyPlaybackState *state, const AppTheme *theme, int ox, int oy, bool is_syncing) {
    float tx = 16.0f + ox;
    float ty = 16.0f + oy;
    float tw = 928.0f;
    float th = 44.0f;

    /* Skeuomorphic raised metallic plate with drop shadow */
    draw_skeuomorphic_panel(tx, ty, tw, th, theme->border_dark, false);

    /* Subtle brushed metal grain lines */
    for (int y = (int)(ty + 4); y < (int)(ty + th - 4); y += 4) {
        vita2d_draw_line(tx + 4, y, tx + tw - 4, y, (theme->border_light & 0x00FFFFFF) | 0x14000000);
    }

    if (s_font) {
        /* Embossed PSVITAMAN brand block: micro-shadow beneath + bright embossed face */
        vita2d_pgf_draw_text(s_font, (int)(tx + 17), (int)(ty + 30), RGBA8(0, 0, 0, 160), 1.05f, "PSVITAMAN");
        vita2d_pgf_draw_text(s_font, (int)(tx + 16), (int)(ty + 29), theme->text_primary, 1.05f, "PSVITAMAN");

        /* Model Stamp */
        vita2d_pgf_draw_text(s_font, (int)(tx + 160), (int)(ty + 29), theme->text_accent, 0.78f, theme->model_stamp);

        /* Acoustic Speaker Grille (7-hole matrix from Image 2) */
        draw_acoustic_grille(tx + 616.0f, ty + 22.0f, 5.5f);

        /* 3D Rotary Volume Knob (Dieter Rams / Teenage Engineering / Image 2 style) */
        draw_rotary_volume_knob(tx + 658.0f, ty + 22.0f, 13.0f, state->volume_percent, theme);

        /* Volume Percentage readout */
        char vol_hud[16];
        snprintf(vol_hud, sizeof(vol_hud), "%d%%", state->volume_percent);
        vita2d_pgf_draw_text(s_font, (int)(tx + 680), (int)(ty + 28), theme->text_primary, 0.76f, vol_hud);

        /* Battery Indicator on Top Right */
        draw_battery_indicator(tx + tw - 58.0f, ty + 22.0f, theme);

        /* Jewel Sync LED with soft radiant bloom */
        draw_skeuomorphic_led(tx + tw - 16.0f, ty + 22.0f, theme->btn_active_led, is_syncing);
    }
}

static void render_cassette_bay(const SpotifyPlaybackState *state, int interpolated_progress_ms, const AppTheme *theme, int ox, int oy) {
    /* Single Clean Cassette Bay Panel with uniform 16px margins */
    float cx = 16.0f + ox;
    float cy = 70.0f + oy;
    float cw = 928.0f;
    float ch = 352.0f;

    /* Skeuomorphic molded chassis panel with ambient drop shadow */
    draw_skeuomorphic_panel(cx, cy, cw, ch, theme->cassette_shell, false);

    /* 5 Outer Cassette Bay Screws with realistic slot highlight */
    draw_screw(cx + 12, cy + 12);
    draw_screw(cx + cw - 12, cy + 12);
    draw_screw(cx + 12, cy + ch - 12);
    draw_screw(cx + cw - 12, cy + ch - 12);
    draw_screw(cx + cw * 0.5f, cy + 12);

    /* Upper Section: Skeuomorphic Retro Illuminated LCD Screen (Dynamic Info) */
    float lx = cx + 24.0f;
    float ly = cy + 12.0f;
    float lw = cw - 48.0f; /* 880.0f */
    float lh = 92.0f;
    draw_retro_lcd_screen(lx, ly, lw, lh, state, interpolated_progress_ms, theme);

    /* Central Clear Acrylic Cassette Window */
    float wx = cx + 80.0f;
    float wy = cy + 110.0f;
    float ww = cw - 160.0f; /* 768.0f */
    float wh = 152.0f;

    /* Deep Sunken Acrylic Window Cavity (Skeuomorphic sunken well with inner shadow) */
    draw_skeuomorphic_panel(wx, wy, ww, wh, theme->cassette_inner, true);

    /* Mechanical Tape Counter (Left of Window) - geared to right spool rotation */
    int counter_val = (int)(s_tape_counter_rotations * 1.5f + (float)interpolated_progress_ms * 0.001f * 0.8f) % 1000;
    draw_tape_counter(cx + 14.0f, wy + (wh - 32.0f) * 0.5f, counter_val, theme);

    /* Stereo LED VU Meters (Right of Window) - synchronized to flutter/playback phase */
    draw_vu_meters(cx + cw - 64.0f, wy + (wh - 100.0f) * 0.5f, state->is_playing, s_flutter_time, theme);

    /* Stamped "AUTO STOP" Header inside window glass */
    if (s_font) {
        const char *auto_stop = "AUTO  STOP";
        int asw = vita2d_pgf_text_width(s_font, 0.70f, auto_stop);
        vita2d_pgf_draw_text(s_font, (int)(wx + (ww - asw) * 0.5f), (int)(wy + 17), (theme->border_light & 0x00FFFFFF) | 0x99000000, 0.70f, auto_stop);
    }

    /* Physics-inspired tape roll geometry */
    float progress_ratio = 0.0f;
    if (state->duration_ms > 0) {
        progress_ratio = (float)interpolated_progress_ms / (float)state->duration_ms;
        if (progress_ratio < 0.0f) progress_ratio = 0.0f;
        if (progress_ratio > 1.0f) progress_ratio = 1.0f;
    }

    /* Dynamic spool radius: 25px bare hub to 68px full pack */
    const float min_r = 25.0f;
    const float max_r = 68.0f;
    const float min_r_sq = min_r * min_r;
    const float max_r_sq = max_r * max_r;
    float left_radius  = sqrtf(min_r_sq + (max_r_sq - min_r_sq) * (1.0f - progress_ratio));
    float right_radius = sqrtf(min_r_sq + (max_r_sq - min_r_sq) * progress_ratio);

    float left_cx  = wx + 145.0f;
    float right_cx = wx + ww - 145.0f;
    float spool_cy = wy + wh * 0.5f - 2.0f;

    /* Dual Rotating Cassette Spools with Dynamic Linear Speed Physics */
    bool left_is_leader = (progress_ratio > 0.985f);
    bool right_is_leader = (progress_ratio < 0.015f);
    draw_spool(left_cx, spool_cy, left_radius, s_left_spool_angle, left_is_leader, theme);
    draw_spool(right_cx, spool_cy, right_radius, s_right_spool_angle, right_is_leader, theme);

    /* Tangential Tape Ribbon Path */
    float roller_y = spool_cy + 50.0f;
    float roller_L_x = left_cx - 38.0f;
    float roller_R_x = right_cx + 38.0f;

    /* Left unspooling ribbon */
    float peel_L_x = left_cx - left_radius + 3.0f;
    float peel_L_y = spool_cy + left_radius * 0.42f;
    for (int t = -2; t <= 2; t++) {
        vita2d_draw_line(peel_L_x + t, peel_L_y, roller_L_x + t, roller_y - 2.0f, theme->tape_brown);
    }

    /* Horizontal magnetic tape ribbon across capstan & playback head notch */
    vita2d_draw_rectangle(roller_L_x - 3.0f, roller_y - 3.0f, (roller_R_x - roller_L_x) + 6.0f, 6.0f, theme->tape_brown);
    vita2d_draw_line(roller_L_x, roller_y - 1.0f, roller_R_x, roller_y - 1.0f, (theme->tape_brown & 0x00FFFFFF) | 0x44000000);

    /* Right intake ribbon */
    float peel_R_x = right_cx + right_radius - 3.0f;
    float peel_R_y = spool_cy + right_radius * 0.42f;
    for (int t = -2; t <= 2; t++) {
        vita2d_draw_line(roller_R_x + t, roller_y - 2.0f, peel_R_x + t, peel_R_y, theme->tape_brown);
    }

    /* Left & Right Flanged Tape Guide Rollers */
    vita2d_draw_fill_circle(roller_L_x, roller_y, 6.0f, RGBA8(195, 200, 210, 255));
    vita2d_draw_fill_circle(roller_L_x, roller_y, 3.5f, RGBA8(110, 115, 125, 255));
    vita2d_draw_fill_circle(roller_L_x, roller_y, 1.5f, RGBA8(240, 245, 250, 255));

    vita2d_draw_fill_circle(roller_R_x, roller_y, 6.0f, RGBA8(195, 200, 210, 255));
    vita2d_draw_fill_circle(roller_R_x, roller_y, 3.5f, RGBA8(110, 115, 125, 255));
    vita2d_draw_fill_circle(roller_R_x, roller_y, 1.5f, RGBA8(240, 245, 250, 255));

    /* Tape Calibration Scale on window glass */
    float center_x = wx + ww * 0.5f;
    if (s_font) {
        vita2d_pgf_draw_text(s_font, (int)(center_x - 52), (int)(spool_cy - 27), (theme->border_light & 0x00FFFFFF) | 0xAA000000, 0.65f, "100  75  50  25   0");
    }
    for (int i = -4; i <= 4; i++) {
        float mark_y = spool_cy + i * 6.5f;
        float tick_len = (i == 0 || i == -4 || i == 4) ? 22.0f : 12.0f;
        vita2d_draw_line(center_x - tick_len * 0.5f, mark_y, center_x + tick_len * 0.5f, mark_y, (theme->border_light & 0x00FFFFFF) | 0x88000000);
    }

    /* Acrylic Window Glare / Glass Reflection System (Analytically bounded to window) */
    /* 1. Interactive specular reflection following device gyro / tilt */
    float gyro_center_x = wx + ww * s_gyro_glare_x;
    float gyro_tilt_dx = s_gyro_glare_tilt;
    for (int bw = -20; bw <= 20; bw += 2) {
        float dist = fabsf((float)bw);
        float factor = 1.0f - (dist / 20.0f);
        int alpha = (int)(factor * factor * 42.0f);
        if (alpha > 0) {
            float xb = gyro_center_x + bw - gyro_tilt_dx * 0.5f;
            float xt = gyro_center_x + bw + gyro_tilt_dx * 0.5f;
            float yb = wy + wh;
            float yt = wy;
            if ((xb < wx && xt < wx) || (xb > wx + ww && xt > wx + ww)) continue;
            if (xb < wx) {
                float t = (wx - xb) / (xt - xb);
                xb = wx;
                yb = (wy + wh) - t * wh;
            }
            if (xt > wx + ww) {
                float t = (xt - (wx + ww)) / (xt - xb);
                xt = wx + ww;
                yt = wy + t * wh;
            }
            vita2d_draw_line(xb, yb, xt, yt, RGBA8(255, 255, 255, (unsigned int)alpha));
        }
    }
    float g_xbc = gyro_center_x - gyro_tilt_dx * 0.5f;
    float g_xtc = gyro_center_x + gyro_tilt_dx * 0.5f;
    float g_ybc = wy + wh;
    float g_ytc = wy;
    if (!((g_xbc < wx && g_xtc < wx) || (g_xbc > wx + ww && g_xtc > wx + ww))) {
        if (g_xbc < wx) {
            float t = (wx - g_xbc) / (g_xtc - g_xbc);
            g_xbc = wx;
            g_ybc = (wy + wh) - t * wh;
        }
        if (g_xtc > wx + ww) {
            float t = (g_xtc - (wx + ww)) / (g_xtc - g_xbc);
            g_xtc = wx + ww;
            g_ytc = wy + t * wh;
        }
        vita2d_draw_line(g_xbc, g_ybc, g_xtc, g_ytc, RGBA8(255, 255, 255, 65));
    }

    /* 2. Occasional dynamic specular glare sweep across the glass */
    if (s_glare_active) {
        float glare_center_x = wx + ww * s_glare_progress;
        float tilt_dx = 90.0f;

        /* Soft specular falloff beam */
        for (int bw = -26; bw <= 26; bw += 2) {
            float dist = fabsf((float)bw);
            float factor = 1.0f - (dist / 26.0f);
            int alpha = (int)(factor * factor * 70.0f);
            if (alpha > 0) {
                float xb = glare_center_x + bw - tilt_dx * 0.5f;
                float xt = glare_center_x + bw + tilt_dx * 0.5f;
                float yb = wy + wh;
                float yt = wy;
                if ((xb < wx && xt < wx) || (xb > wx + ww && xt > wx + ww)) continue;
                if (xb < wx) {
                    float t = (wx - xb) / (xt - xb);
                    xb = wx;
                    yb = (wy + wh) - t * wh;
                }
                if (xt > wx + ww) {
                    float t = (xt - (wx + ww)) / (xt - xb);
                    xt = wx + ww;
                    yt = wy + t * wh;
                }
                vita2d_draw_line(xb, yb, xt, yt, RGBA8(255, 255, 255, (unsigned int)alpha));
            }
        }

        /* Bright specular glint edge */
        float xbc = glare_center_x - tilt_dx * 0.5f;
        float xtc = glare_center_x + tilt_dx * 0.5f;
        float ybc = wy + wh;
        float ytc = wy;
        if (!((xbc < wx && xtc < wx) || (xbc > wx + ww && xtc > wx + ww))) {
            if (xbc < wx) {
                float t = (wx - xbc) / (xtc - xbc);
                xbc = wx;
                ybc = (wy + wh) - t * wh;
            }
            if (xtc > wx + ww) {
                float t = (xtc - (wx + ww)) / (xtc - xbc);
                xtc = wx + ww;
                ytc = wy + t * wh;
            }
            vita2d_draw_line(xbc, ybc, xtc, ytc, RGBA8(255, 255, 255, 110));
            vita2d_draw_line(xbc + 1.0f, ybc, xtc + 1.0f, ytc, RGBA8(255, 255, 255, 85));
        }
    }

    /* Cassette Head Trapezoid Notch */
    float tz_w = 400.0f;
    float tz_x = cx + (cw - tz_w) * 0.5f;
    float tz_y = cy + 270.0f;
    float tz_h = 22.0f;
    draw_beveled_box(tz_x, tz_y, tz_w, tz_h, theme->cassette_inner, theme->border_dark, theme->border_light);

    /* Capstan Holes */
    vita2d_draw_fill_circle(tz_x + 55.0f, tz_y + 11.0f, 6.0f, RGBA8(180, 185, 195, 255));
    vita2d_draw_fill_circle(tz_x + 55.0f, tz_y + 11.0f, 3.0f, RGBA8(10, 12, 16, 255));
    vita2d_draw_fill_circle(tz_x + tz_w - 55.0f, tz_y + 11.0f, 6.0f, RGBA8(180, 185, 195, 255));
    vita2d_draw_fill_circle(tz_x + tz_w - 55.0f, tz_y + 11.0f, 3.0f, RGBA8(10, 12, 16, 255));

    /* Tape Head Exposure Window & Magnetic Ribbon */
    vita2d_draw_rectangle(tz_x + 100.0f, tz_y + 4.0f, tz_w - 200.0f, 14.0f, RGBA8(8, 10, 14, 255));
    vita2d_draw_rectangle(tz_x + 110.0f, tz_y + 8.0f, tz_w - 220.0f, 6.0f, theme->tape_brown);

    /* Lower HUD Inset Strip (High-fidelity Equipment Specification Plate) */
    float hx = cx + 24.0f;
    float hy = cy + 304.0f;
    float hw = cw - 48.0f; /* 880.0f */
    float hh = 32.0f;

    draw_skeuomorphic_panel(hx, hy, hw, hh, RGBA8(12, 14, 18, 255), true);

    if (s_font) {
        /* Equipment Specification Typography */
        const char *dev_name = (strlen(state->device_name) > 0) ? state->device_name : "NO ACTIVE DEVICE";
        char dev_str[128];
        snprintf(dev_str, sizeof(dev_str), "OUTPUT: %s", dev_name);
        vita2d_pgf_draw_text(s_font, (int)(hx + 18), (int)(hy + 22), theme->text_muted, 0.72f, dev_str);

        const char *tech_str = "HIGH FIDELITY STEREO CASSETTE MECHANISM";
        int tw_tech = vita2d_pgf_text_width(s_font, 0.72f, tech_str);
        vita2d_pgf_draw_text(s_font, (int)(hx + (hw - tw_tech) * 0.5f), (int)(hy + 22), (theme->border_light & 0x00FFFFFF) | 0x88000000, 0.72f, tech_str);

        const char *dolby_str = "DOLBY B-C NR  •  TYPE II";
        int tw_dolby = vita2d_pgf_text_width(s_font, 0.72f, dolby_str);
        vita2d_pgf_draw_text(s_font, (int)(hx + hw - tw_dolby - 18), (int)(hy + 22), theme->text_muted, 0.72f, dolby_str);
    }
}

static void draw_vita_button_glyph(int btn_idx, float gx, float gy) {
    if (btn_idx == BTN_INDEX_PREV || btn_idx == BTN_INDEX_NEXT) {
        /* Shoulder Trigger Pill (L / R) */
        float tw = 20.0f;
        float th = 13.0f;
        float tx = gx - 6.0f;
        float ty = gy - 7.0f;

        /* Drop shadow */
        vita2d_draw_rectangle(tx, ty + 1.0f, tw, th, RGBA8(0, 0, 0, 90));
        /* Beveled trigger body */
        vita2d_draw_rectangle(tx, ty, tw, th - 1.0f, RGBA8(26, 30, 40, 255));
        /* Subtle trigger highlight and lowlight */
        vita2d_draw_line(tx, ty, tx + tw - 1.0f, ty, RGBA8(90, 105, 125, 255));
        vita2d_draw_line(tx, ty, tx, ty + th - 1.0f, RGBA8(80, 95, 115, 255));
        vita2d_draw_line(tx + tw - 1.0f, ty, tx + tw - 1.0f, ty + th - 1.0f, RGBA8(14, 18, 24, 255));
        vita2d_draw_line(tx, ty + th - 1.0f, tx + tw, ty + th - 1.0f, RGBA8(14, 18, 24, 255));

        if (s_font) {
            const char *label = (btn_idx == BTN_INDEX_PREV) ? "L" : "R";
            int w = vita2d_pgf_text_width(s_font, 0.58f, label);
            vita2d_pgf_draw_text(s_font, (int)(tx + (tw - w) * 0.5f), (int)(ty + 9.5f), RGBA8(230, 235, 245, 230), 0.58f, label);
        }
    } else {
        /* Circular PlayStation Face Button (X, O, Square, Triangle) */
        float r = 7.5f;
        /* Shadow */
        vita2d_draw_fill_circle(gx, gy + 1.0f, r + 0.5f, RGBA8(0, 0, 0, 90));
        /* Dark circular bezel */
        vita2d_draw_fill_circle(gx, gy, r, RGBA8(22, 26, 34, 255));
        vita2d_draw_fill_circle(gx, gy, r - 1.0f, RGBA8(32, 38, 48, 255));
        /* Inner face highlight */
        vita2d_draw_line(gx - r * 0.5f, gy - r + 1.5f, gx + r * 0.5f, gy - r + 1.5f, RGBA8(255, 255, 255, 45));

        switch (btn_idx) {
            case BTN_INDEX_PLAY: { /* Cross (X) - PlayStation Blue */
                unsigned int c = RGBA8(85, 155, 255, 255);
                vita2d_draw_line(gx - 3.2f, gy - 3.2f, gx + 3.2f, gy + 3.2f, c);
                vita2d_draw_line(gx - 3.2f + 0.7f, gy - 3.2f, gx + 3.2f + 0.7f, gy + 3.2f, c);
                vita2d_draw_line(gx - 3.2f, gy + 3.2f, gx + 3.2f, gy - 3.2f, c);
                vita2d_draw_line(gx - 3.2f + 0.7f, gy + 3.2f, gx + 3.2f + 0.7f, gy - 3.2f, c);
                break;
            }
            case BTN_INDEX_PAUSE: { /* Circle (O) - PlayStation Red */
                unsigned int c = RGBA8(255, 80, 80, 255);
                vita2d_draw_fill_circle(gx, gy, 4.3f, c);
                vita2d_draw_fill_circle(gx, gy, 2.7f, RGBA8(32, 38, 48, 255));
                break;
            }
            case BTN_INDEX_SHUFFLE: { /* Square - PlayStation Pink */
                unsigned int c = RGBA8(255, 115, 185, 255);
                vita2d_draw_rectangle(gx - 3.5f, gy - 3.5f, 7.0f, 7.0f, c);
                vita2d_draw_rectangle(gx - 2.0f, gy - 2.0f, 4.0f, 4.0f, RGBA8(32, 38, 48, 255));
                break;
            }
            case BTN_INDEX_REPEAT: { /* Triangle - PlayStation Green */
                unsigned int c = RGBA8(50, 220, 130, 255);
                vita2d_draw_line(gx, gy - 4.2f, gx - 4.0f, gy + 3.5f, c);
                vita2d_draw_line(gx, gy - 4.2f, gx + 4.0f, gy + 3.5f, c);
                vita2d_draw_line(gx - 4.0f, gy + 3.5f, gx + 4.0f, gy + 3.5f, c);
                vita2d_draw_line(gx, gy - 3.2f, gx - 3.0f, gy + 2.5f, c);
                vita2d_draw_line(gx, gy - 3.2f, gx + 3.0f, gy + 2.5f, c);
                vita2d_draw_line(gx - 3.0f, gy + 2.5f, gx + 3.0f, gy + 2.5f, c);
                break;
            }
        }
    }
}

static void render_transport_bar(const SpotifyPlaybackState *state, const InputState *input, const AppTheme *theme, int ox, int oy) {
    /* Sunken mechanical button bay trench across the bottom of the device (Image 1 & Image 2 style) */
    float bx_bay = 16.0f + ox;
    float by_bay = 432.0f + oy;
    float bw_bay = 928.0f;
    float bh_bay = 96.0f;

    /* Skeuomorphic dark sunken keyboard well */
    draw_skeuomorphic_panel(bx_bay, by_bay, bw_bay, bh_bay, RGBA8(18, 20, 26, 255), true);

    const DeckButtonRect *btns = input_get_button_rects();

    for (int i = 0; i < DECK_BTN_COUNT; i++) {
        const DeckButtonRect *b = &btns[i];
        bool is_pressed = (input->pressed_button == i);

        float bx = b->x + ox;
        float by = b->y + oy;
        float bw = b->w;
        float bh = b->h;

        unsigned int key_color = theme->btn_normal;

        /* Check mechanical latched "ON" state */
        bool has_led = (i == BTN_INDEX_SHUFFLE || i == BTN_INDEX_REPEAT ||
                        i == BTN_INDEX_PLAY || i == BTN_INDEX_PAUSE);
        bool is_active = false;
        if (i == BTN_INDEX_PLAY && state->is_playing) is_active = true;
        if (i == BTN_INDEX_PAUSE && !state->is_playing && state->duration_ms > 0) is_active = true;
        if (i == BTN_INDEX_SHUFFLE && state->shuffle_state) is_active = true;
        if (i == BTN_INDEX_REPEAT && state->repeat_state != REPEAT_OFF) is_active = true;

        /* Calculate physical displacement for sunken/latched button */
        float y_disp = 0.0f;
        if (is_pressed) {
            y_disp = is_active ? 7.0f : 4.0f;
        } else if (is_active) {
            y_disp = 5.0f;
        }

        /* Draw tactile cantilevered piano key with distinct mechanical latched ON state (Image 2 style) */
        draw_skeuomorphic_key(bx, by, bw, bh, key_color, is_pressed, is_active, theme->btn_active_led);

        /* Inset glowing jewel LED in togglable buttons (Play, Pause, Shuffle, Repeat) */
        if (has_led) {
            float led_x = bx + bw - 15.0f;
            float led_y = by + y_disp + 11.0f;
            draw_skeuomorphic_led(led_x, led_y, theme->btn_active_led, is_active);
        }

        /* Centralized coordinates for 20% enlarged vector icon inside the concave finger cup */
        float icx = bx + bw * 0.5f;
        float icy = by + y_disp + bh * 0.48f;
        unsigned int icon_color = is_active ? theme->btn_active_led : theme->text_primary;

        switch (i) {
            case BTN_INDEX_PLAY:
                draw_icon_play(icx, icy, 24.0f, icon_color);
                break;
            case BTN_INDEX_PAUSE:
                draw_icon_pause(icx, icy, 22.0f, 22.0f, icon_color);
                break;
            case BTN_INDEX_PREV:
                draw_icon_prev(icx, icy, 24.0f, icon_color);
                break;
            case BTN_INDEX_NEXT:
                draw_icon_next(icx, icy, 24.0f, icon_color);
                break;
            case BTN_INDEX_SHUFFLE:
                draw_icon_shuffle(icx, icy, 24.0f, icon_color);
                break;
            case BTN_INDEX_REPEAT:
                draw_icon_repeat(icx, icy, 24.0f, state->repeat_state == REPEAT_TRACK, icon_color);
                break;
        }

        /* PlayStation Vita button glyph in the bottom-left of each key */
        float gx = bx + 16.0f;
        float gy = by + y_disp + bh - 15.0f;
        draw_vita_button_glyph(i, gx, gy);
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
