/**
 * PSVitaman - User Interface & Cassette Rendering Engine
 */

#ifndef PSVITAMAN_UI_H
#define PSVITAMAN_UI_H

#include "spotify.h"
#include "input.h"
#include "config.h"
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
              const InputState *input, const AppConfig *config, bool is_syncing);

#endif /* PSVITAMAN_UI_H */
