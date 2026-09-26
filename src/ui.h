/**
 * PSVitaman - User Interface & Cassette Rendering Engine
 */

#ifndef PSVITAMAN_UI_H
#define PSVITAMAN_UI_H

#include "spotify.h"
#include "input.h"
#include "config.h"
#include "error.h"
#include <stdbool.h>

#define SCREEN_WIDTH  960
#define SCREEN_HEIGHT 544

/* Initialize UI subsystem, load system fonts and textures */
bool ui_init(void);

/* Free UI resources */
void ui_cleanup(void);

/* Update UI animation states (spool rotation, marquee offset, etc.) */
void ui_update(float delta_time, const SpotifyPlaybackState *state, int interpolated_progress_ms);

/* Render main Walkman interface frame */
void ui_render(const SpotifyPlaybackState *state, int interpolated_progress_ms,
              const InputState *input, const AppConfig *config, bool is_syncing,
              bool show_qr_overlay, const AppError *error);

typedef enum {
    THEME_TPS_L2 = 0,       /* 1979 Original Walkman (Blue/Silver + Orange accent button) */
    THEME_SPORTS_YELLOW,    /* 1983 Sports Walkman WM-F5 (Vivid Yellow/Black + Turquoise button) */
    THEME_GRAPHITE_DD,      /* 1982 Walkman WM-DD / WM-2 (Charcoal Anthracite / Chrome Platinum) */
    THEME_STEALTH_OLED,     /* Minimalist AMOLED True Black / Neon Green (0% power on OLED) */
    THEME_WM2_RED,          /* 1981 Walkman WM-2 (Vivid Japanese Red / Matte Black + Amber button) */
    THEME_WM_D6C_PRO,       /* 1984 Professional WM-D6C (Studio Matte Black / Dolby Gold + Studio Red) */
    THEME_MY_FIRST_SONY,    /* 1987 My First Sony (Pop Red / Cobalt Blue + Blue Hubs & Yellow Teeth) */
    THEME_CHAMPAGNE_GOLD,   /* 1989 10th Anniversary WM-701C (Titanium Champagne Gold / Royal Navy) */
    THEME_COUNT
} ThemeId;

/* Theme management */
void ui_set_theme(int theme_id);
int ui_get_theme(void);
void ui_cycle_theme(void);
void ui_cycle_theme_prev(void);
const char *ui_get_theme_name(int theme_id);

#endif /* PSVITAMAN_UI_H */
