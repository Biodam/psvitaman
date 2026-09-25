/**
 * PSVitaman - Background Worker & Concurrency Engine
 */

#ifndef PSVITAMAN_WORKER_H
#define PSVITAMAN_WORKER_H

#include "spotify.h"
#include "config.h"
#include <stdbool.h>

typedef enum {
    CMD_NONE = 0,
    CMD_TOGGLE_PLAY_PAUSE,
    CMD_SKIP_NEXT,
    CMD_SKIP_PREV,
    CMD_VOLUME_UP,
    CMD_VOLUME_DOWN,
    CMD_TOGGLE_SHUFFLE,
    CMD_CYCLE_REPEAT,
    CMD_FORCE_REFRESH
} WorkerCommand;

/* Initialize worker subsystem and spawn thread */
bool worker_start(const AppConfig *config);

/* Signal worker to stop and wait for thread exit */
void worker_stop(void);

/* Enqueue a command from the UI/Input thread */
bool worker_enqueue_command(WorkerCommand cmd);

/* Get snapshot of current playback state + smooth interpolated progress */
void worker_get_playback_state(SpotifyPlaybackState *out_state, int *out_interpolated_progress_ms);

/* Check if worker is currently connected / syncing */
bool worker_is_syncing(void);

/* Check if initial authentication succeeded */
bool worker_is_authenticated(void);

#endif /* PSVITAMAN_WORKER_H */
