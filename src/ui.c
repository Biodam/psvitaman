/**
 * PSVitaman - User Interface & Cassette Rendering Engine Implementation
 */

#include "ui.h"
#include "utils.h"
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

/* Theme Color Palette */
#define COLOR_CHASSIS_BG      RGBA8(22, 26, 34, 255)
#define COLOR_TOP_BAR_BG      RGBA8(36, 42, 54, 255)
#define COLOR_BORDER_LIGHT    RGBA8(85, 96, 118, 255)
#define COLOR_BORDER_DARK     RGBA8(14, 16, 22, 255)

#define COLOR_BTN_NORMAL      RGBA8(48, 55, 70, 255)
#define COLOR_BTN_PRESSED     RGBA8(28, 32, 42, 255)
#define COLOR_BTN_ACTIVE      RGBA8(30, 215, 96, 255) /* Spotify Green */
#define COLOR_BTN_BORDER_HI   RGBA8(90, 102, 125, 255)
#define COLOR_BTN_BORDER_LO   RGBA8(16, 18, 24, 255)
#define COLOR_TEXT_WHITE      RGBA8(240, 242, 245, 255)
#define COLOR_TEXT_MUTED      RGBA8(150, 160, 175, 255)
#define COLOR_TEXT_GREEN      RGBA8(30, 215, 96, 255)
#define COLOR_TEXT_AMBER      RGBA8(245, 170, 45, 255)

#define COLOR_CASSETTE_SHELL  RGBA8(32, 36, 46, 255)
#define COLOR_CASSETTE_INNER  RGBA8(20, 23, 30, 255)
#define COLOR_CASSETTE_LABEL  RGBA8(242, 239, 230, 255) /* Vintage cream paper */
#define COLOR_LABEL_RED       RGBA8(225, 55, 45, 255)
#define COLOR_LABEL_BLUE      RGBA8(35, 105, 215, 255)
#define COLOR_LABEL_TEXT      RGBA8(30, 32, 36, 255)
#define COLOR_TAPE_BROWN      RGBA8(55, 38, 24, 255)
#define COLOR_SPOOL_HUB       RGBA8(245, 245, 245, 255)
#define COLOR_SPOOL_GEAR      RGBA8(160, 165, 175, 255)
#define COLOR_SCREW           RGBA8(170, 175, 185, 255)

static vita2d_pgf *s_font = NULL;

/* Animation State */
static float s_spool_angle = 0.0f;
static float s_marquee_offset = 0.0f;
static char s_prev_track[SPOTIFY_TRACK_NAME_MAX] = {0};

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
    (void)interpolated_progress_ms;

    /* Rotate spools during active playback */
    if (state && state->is_playing) {
        s_spool_angle += 140.0f * delta_time;
        if (s_spool_angle >= 360.0f) {
            s_spool_angle -= 360.0f;
        }
    }

    /* Reset marquee offset if track changed */
    if (state && strcmp(state->track_name, s_prev_track) != 0) {
        utils_safe_strncpy(s_prev_track, state->track_name, sizeof(s_prev_track));
        s_marquee_offset = 0.0f;
    }

    /* Marquee scroll animation */
    s_marquee_offset += 45.0f * delta_time;
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

static void render_transport_bar(const SpotifyPlaybackState *state, const InputState *input) {
    /* Top Bar brushed metal chassis */
    draw_beveled_box(0, 0, SCREEN_WIDTH, 102, COLOR_TOP_BAR_BG, COLOR_BORDER_LIGHT, COLOR_BORDER_DARK);

    /* Subtle horizontal brushed texture lines */
    for (int y = 4; y < 100; y += 4) {
        vita2d_draw_line(0, y, SCREEN_WIDTH, y, RGBA8(46, 54, 68, 255));
    }

    const DeckButtonRect *btns = input_get_button_rects();

    for (int i = 0; i < DECK_BTN_COUNT; i++) {
        const DeckButtonRect *b = &btns[i];
        bool is_pressed = (input->pressed_button == i);

        /* Determine if button has toggled state (e.g., Shuffle on, Repeat on, Playing) */
        bool is_toggled = false;
        if (i == BTN_INDEX_PLAY_PAUSE && state->is_playing) is_toggled = true;
        if (i == BTN_INDEX_SHUFFLE && state->shuffle_state) is_toggled = true;
        if (i == BTN_INDEX_REPEAT && state->repeat_state != REPEAT_OFF) is_toggled = true;

        float bx = b->x;
        float by = b->y + (is_pressed ? 3 : 0);
        float bw = b->w;
        float bh = b->h - (is_pressed ? 1 : 0);

        unsigned int fill_color = is_pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
        unsigned int hi_color = is_pressed ? COLOR_BTN_BORDER_LO : COLOR_BTN_BORDER_HI;
        unsigned int lo_color = is_pressed ? COLOR_BTN_BORDER_HI : COLOR_BTN_BORDER_LO;

        draw_beveled_box(bx, by, bw, bh, fill_color, hi_color, lo_color);

        /* Indicator LED in top corner of togglable buttons */
        if (i == BTN_INDEX_SHUFFLE || i == BTN_INDEX_REPEAT || i == BTN_INDEX_PLAY_PAUSE) {
            unsigned int led_color = is_toggled ? COLOR_BTN_ACTIVE : RGBA8(60, 70, 85, 255);
            vita2d_draw_fill_circle(bx + bw - 14, by + 12, 4, led_color);
        }

        /* Button Label */
        const char *display_label = b->label;
        if (i == BTN_INDEX_PLAY_PAUSE) {
            display_label = state->is_playing ? "PAUSE" : "PLAY";
        } else if (i == BTN_INDEX_REPEAT) {
            if (state->repeat_state == REPEAT_TRACK) display_label = "REP [1]";
            else if (state->repeat_state == REPEAT_CONTEXT) display_label = "REPEAT";
            else display_label = "REP OFF";
        }

        if (s_font) {
            int tw = vita2d_pgf_text_width(s_font, 1.0f, display_label);
            int tx = (int)(bx + (bw - tw) / 2);
            int ty = (int)(by + 34);
            unsigned int text_col = is_toggled ? COLOR_TEXT_GREEN : COLOR_TEXT_WHITE;
            vita2d_pgf_draw_text(s_font, tx, ty, text_col, 1.0f, display_label);

            /* Hotkey hint badge under button text */
            int hw = vita2d_pgf_text_width(s_font, 0.7f, b->hotkey_hint);
            int hx = (int)(bx + (bw - hw) / 2);
            int hy = (int)(by + 54);
            vita2d_pgf_draw_text(s_font, hx, hy, COLOR_TEXT_MUTED, 0.7f, b->hotkey_hint);
        }
    }
}

static void draw_screw(float x, float y) {
    vita2d_draw_fill_circle(x, y, 6, COLOR_SCREW);
    vita2d_draw_fill_circle(x, y, 4, RGBA8(140, 145, 155, 255));
    vita2d_draw_line(x - 4, y, x + 4, y, RGBA8(70, 75, 85, 255));
}

static void draw_spool(float cx, float cy, float outer_radius, float angle_deg) {
    /* Outer spooled magnetic tape */
    if (outer_radius > 26.0f) {
        vita2d_draw_fill_circle(cx, cy, outer_radius, COLOR_TAPE_BROWN);
    }

    /* White plastic cassette hub */
    vita2d_draw_fill_circle(cx, cy, 26.0f, COLOR_SPOOL_HUB);
    vita2d_draw_fill_circle(cx, cy, 14.0f, COLOR_CASSETTE_INNER);

    /* 6 teeth / spokes radiating outward */
    for (int i = 0; i < 6; i++) {
        float rad = (angle_deg + i * 60.0f) * (3.14159265f / 180.0f);
        float x1 = cx + cosf(rad) * 14.0f;
        float y1 = cy + sinf(rad) * 14.0f;
        float x2 = cx + cosf(rad) * 26.0f;
        float y2 = cy + sinf(rad) * 26.0f;
        vita2d_draw_line(x1, y1, x2, y2, COLOR_SPOOL_GEAR);
    }
}

static void render_cassette_bay(const SpotifyPlaybackState *state, int interpolated_progress_ms, bool is_syncing) {
    /* Main Cassette Shell */
    float cx = 60.0f;
    float cy = 114.0f;
    float cw = 840.0f;
    float ch = 416.0f;

    draw_beveled_box(cx, cy, cw, ch, COLOR_CASSETTE_SHELL, COLOR_BORDER_LIGHT, COLOR_BORDER_DARK);

    /* Corner Screws */
    draw_screw(cx + 20, cy + 20);
    draw_screw(cx + cw - 20, cy + 20);
    draw_screw(cx + 20, cy + ch - 20);
    draw_screw(cx + cw - 20, cy + ch - 20);
    draw_screw(cx + cw / 2, cy + ch - 18);

    /* Upper Cassette Label */
    float lx = cx + 55.0f;
    float ly = cy + 26.0f;
    float lw = cw - 110.0f;
    float lh = 120.0f;

    draw_beveled_box(lx, ly, lw, lh, COLOR_CASSETTE_LABEL, RGBA8(255, 255, 255, 255), RGBA8(190, 185, 175, 255));

    /* Retro Red & Blue racing stripe across top of label */
    vita2d_draw_rectangle(lx + 2, ly + 6, lw - 4, 3, COLOR_LABEL_RED);
    vita2d_draw_rectangle(lx + 2, ly + 9, lw - 4, 3, COLOR_LABEL_BLUE);

    if (s_font) {
        /* Vintage cassette brand typography */
        vita2d_pgf_draw_text(s_font, (int)(lx + 16), (int)(ly + 32), COLOR_LABEL_RED, 0.85f, "A");
        vita2d_pgf_draw_text(s_font, (int)(lx + 36), (int)(ly + 32), COLOR_LABEL_TEXT, 0.75f, "PSVITAMAN • C-90 HIGH BIAS");
        vita2d_pgf_draw_text(s_font, (int)(lx + lw - 140), (int)(ly + 32), COLOR_LABEL_TEXT, 0.70f, "STEREO • 120µs");

        /* Track Title Label with Marquee Scrolling */
        const char *track_title = (strlen(state->track_name) > 0) ? state->track_name : "Playback Idle / Stopped";
        int title_w = vita2d_pgf_text_width(s_font, 1.25f, track_title);

        float clip_x = lx + 16.0f;
        float clip_y = ly + 38.0f;
        float clip_w = lw - 32.0f;
        float clip_h = 36.0f;

        vita2d_set_clip_rectangle((int)clip_x, (int)clip_y, (int)clip_w, (int)clip_h);

        if (title_w > clip_w) {
            float total_scroll = title_w + 80.0f;
            float cur_x = clip_x - fmodf(s_marquee_offset, total_scroll);
            vita2d_pgf_draw_text(s_font, (int)cur_x, (int)(clip_y + 28), COLOR_LABEL_TEXT, 1.25f, track_title);
            vita2d_pgf_draw_text(s_font, (int)(cur_x + total_scroll), (int)(clip_y + 28), COLOR_LABEL_TEXT, 1.25f, track_title);
        } else {
            vita2d_pgf_draw_text(s_font, (int)clip_x, (int)(clip_y + 28), COLOR_LABEL_TEXT, 1.25f, track_title);
        }

        vita2d_disable_clipping();

        /* Artist & Album line */
        char artist_album[512] = {0};
        if (strlen(state->artist_name) > 0) {
            if (strlen(state->album_name) > 0) {
                snprintf(artist_album, sizeof(artist_album), "%s — %s", state->artist_name, state->album_name);
            } else {
                utils_safe_strncpy(artist_album, state->artist_name, sizeof(artist_album));
            }
        } else {
            utils_safe_strncpy(artist_album, "Connect Spotify from Phone, PC, or Console", sizeof(artist_album));
        }

        vita2d_set_clip_rectangle((int)clip_x, (int)(ly + 76), (int)clip_w, 30);
        vita2d_pgf_draw_text(s_font, (int)clip_x, (int)(ly + 98), RGBA8(75, 80, 90, 255), 0.90f, artist_album);
        vita2d_disable_clipping();
    }

    /* Cassette Center Window */
    float wx = cx + 160.0f;
    float wy = cy + 158.0f;
    float ww = cw - 320.0f;
    float wh = 175.0f;

    draw_beveled_box(wx, wy, ww, wh, COLOR_CASSETTE_INNER, COLOR_BORDER_DARK, COLOR_BORDER_LIGHT);

    /* Tape Roll Proportions based on current track progress */
    float progress_ratio = 0.0f;
    if (state->duration_ms > 0) {
        progress_ratio = (float)interpolated_progress_ms / (float)state->duration_ms;
        if (progress_ratio < 0.0f) progress_ratio = 0.0f;
        if (progress_ratio > 1.0f) progress_ratio = 1.0f;
    }

    /* Physics-inspired reel geometry: area ~ tape length, so radius ~ sqrt(ratio) */
    float left_radius  = 26.0f + 42.0f * sqrtf(1.0f - progress_ratio);
    float right_radius = 26.0f + 42.0f * sqrtf(progress_ratio);

    float left_cx  = wx + 95.0f;
    float right_cx = wx + ww - 95.0f;
    float spool_cy = wy + wh / 2.0f - 8.0f;

    /* Draw bottom magnetic tape ribbon across guide rollers */
    vita2d_draw_rectangle(left_cx - 40, spool_cy + 48, (right_cx - left_cx) + 80, 8, COLOR_TAPE_BROWN);
    vita2d_draw_fill_circle(left_cx - 35, spool_cy + 52, 6, RGBA8(160, 165, 175, 255));
    vita2d_draw_fill_circle(right_cx + 35, spool_cy + 52, 6, RGBA8(160, 165, 175, 255));

    /* Dual Rotating Spools */
    draw_spool(left_cx, spool_cy, left_radius, s_spool_angle);
    draw_spool(right_cx, spool_cy, right_radius, s_spool_angle);

    /* Center tape index scale lines on window */
    float center_x = wx + ww / 2.0f;
    for (int i = -3; i <= 3; i++) {
        float mark_y = spool_cy + i * 8.0f;
        vita2d_draw_line(center_x - 16, mark_y, center_x + 16, mark_y, RGBA8(100, 110, 130, 200));
    }

    /* Lower HUD Strip: Progress bar, LCD counter, device name, volume */
    float hx = cx + 40.0f;
    float hy = cy + ch - 62.0f;
    float hw = cw - 80.0f;
    float hh = 46.0f;

    draw_beveled_box(hx, hy, hw, hh, RGBA8(16, 19, 25, 255), COLOR_BORDER_DARK, COLOR_BORDER_LIGHT);

    /* Thin Progress Bar along top edge of HUD */
    vita2d_draw_rectangle(hx + 4, hy + 3, hw - 8, 4, RGBA8(32, 38, 50, 255));
    if (progress_ratio > 0.0f) {
        vita2d_draw_rectangle(hx + 4, hy + 3, (hw - 8) * progress_ratio, 4, COLOR_BTN_ACTIVE);
    }

    if (s_font) {
        /* Time Readout: MM:SS / MM:SS */
        char cur_time[16], total_time[16], time_hud[40];
        utils_format_time_ms(interpolated_progress_ms, cur_time, sizeof(cur_time));
        utils_format_time_ms(state->duration_ms, total_time, sizeof(total_time));
        snprintf(time_hud, sizeof(time_hud), "⏱ %s / %s", cur_time, total_time);
        vita2d_pgf_draw_text(s_font, (int)(hx + 14), (int)(hy + 30), COLOR_TEXT_GREEN, 0.90f, time_hud);

        /* Volume Display */
        char vol_hud[32];
        snprintf(vol_hud, sizeof(vol_hud), "VOL: %d%%", state->volume_percent);
        vita2d_pgf_draw_text(s_font, (int)(hx + 240), (int)(hy + 30), COLOR_TEXT_WHITE, 0.85f, vol_hud);

        /* Target Spotify Device */
        char dev_hud[128];
        snprintf(dev_hud, sizeof(dev_hud), "DEVICE: %s", (strlen(state->device_name) > 0) ? state->device_name : "None (Idle)");
        vita2d_pgf_draw_text(s_font, (int)(hx + 380), (int)(hy + 30), COLOR_TEXT_AMBER, 0.85f, dev_hud);

        /* Live Sync / Pulse Dot */
        unsigned int sync_color = is_syncing ? COLOR_BTN_ACTIVE : RGBA8(50, 60, 75, 255);
        vita2d_draw_fill_circle(hx + hw - 20, hy + 24, 5, sync_color);
    }
}

static void render_setup_guide(const AppConfig *config) {
    (void)config;
    /* Walkman styled setup walkthrough for missing credentials */
    vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_CHASSIS_BG);

    draw_beveled_box(80, 40, SCREEN_WIDTH - 160, SCREEN_HEIGHT - 80,
                    RGBA8(30, 36, 48, 255), COLOR_BORDER_LIGHT, COLOR_BORDER_DARK);

    if (s_font) {
        vita2d_pgf_draw_text(s_font, 120, 95, COLOR_TEXT_GREEN, 1.4f, "PSVITAMAN • SPOTIFY REMOTE");
        vita2d_pgf_draw_text(s_font, 120, 130, COLOR_TEXT_AMBER, 1.0f, "Initial Configuration Required");

        vita2d_pgf_draw_text(s_font, 120, 175, COLOR_TEXT_WHITE, 0.85f,
                             "A template configuration file was automatically created at:");
        vita2d_pgf_draw_text(s_font, 140, 205, COLOR_TEXT_GREEN, 0.85f,
                             "ux0:data/psvitaman/config.ini");

        vita2d_pgf_draw_text(s_font, 120, 245, COLOR_TEXT_WHITE, 0.85f,
                             "To connect your Spotify account:");
        vita2d_pgf_draw_text(s_font, 140, 275, COLOR_TEXT_MUTED, 0.80f,
                             "1. Create a free app at https://developer.spotify.com/dashboard");
        vita2d_pgf_draw_text(s_font, 140, 305, COLOR_TEXT_MUTED, 0.80f,
                             "2. Set Redirect URI to: http://127.0.0.1:8888/callback");
        vita2d_pgf_draw_text(s_font, 140, 335, COLOR_TEXT_MUTED, 0.80f,
                             "3. Run 'python tools/get_token.py' on PC to generate your refresh_token.");
        vita2d_pgf_draw_text(s_font, 140, 365, COLOR_TEXT_MUTED, 0.80f,
                             "4. Copy your credentials into ux0:data/psvitaman/config.ini via VitaShell.");

        vita2d_pgf_draw_text(s_font, 120, 420, COLOR_TEXT_GREEN, 0.90f,
                             "Press [START] on your Vita to reload config once updated.");
    }
}

void ui_render(const SpotifyPlaybackState *state, int interpolated_progress_ms,
              const InputState *input, const AppConfig *config, bool is_syncing) {
    vita2d_start_drawing();
    vita2d_clear_screen();

    if (!config->is_valid) {
        render_setup_guide(config);
    } else {
        render_transport_bar(state, input);
        render_cassette_bay(state, interpolated_progress_ms, is_syncing);
    }

    vita2d_end_drawing();
    vita2d_swap_buffers();
}
