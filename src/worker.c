/**
 * PSVitaman - Background Worker & Concurrency Engine Implementation
 */

#include "worker.h"
#include "spotify.h"
#include "utils.h"
#include "logger.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/net/netctl.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#endif

#define CMD_QUEUE_SIZE 16
#define POLL_INTERVAL_MS 1500

static AppConfig g_config;
static volatile bool g_running = false;
static volatile bool g_authenticated = false;
static volatile bool g_syncing = false;

static SpotifyPlaybackState g_playback_state;
static uint64_t g_state_updated_tick = 0;

static char g_access_token[512] = {0};
static uint64_t g_token_expiry_tick = 0;

/* Circular Command Queue */
static WorkerCommand g_cmd_queue[CMD_QUEUE_SIZE];
static int g_cmd_head = 0;
static int g_cmd_tail = 0;

#if defined(__psp2__) || defined(__VITA__)
static SceUID g_thid = -1;
static SceUID g_mutex = -1;
#else
static pthread_t g_thread;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static uint64_t get_time_ms(void) {
#if defined(__psp2__) || defined(__VITA__)
    return sceKernelGetProcessTimeWide() / 1000;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

static void lock_mutex(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (g_mutex >= 0) sceKernelLockMutex(g_mutex, 1, NULL);
#else
    pthread_mutex_lock(&g_mutex);
#endif
}

static void unlock_mutex(void) {
#if defined(__psp2__) || defined(__VITA__)
    if (g_mutex >= 0) sceKernelUnlockMutex(g_mutex, 1);
#else
    pthread_mutex_unlock(&g_mutex);
#endif
}

static void sleep_ms(int ms) {
#if defined(__psp2__) || defined(__VITA__)
    sceKernelDelayThread(ms * 1000);
#else
    usleep(ms * 1000);
#endif
}

static bool pop_command(WorkerCommand *cmd) {
    lock_mutex();
    if (g_cmd_head == g_cmd_tail) {
        unlock_mutex();
        return false;
    }
    *cmd = g_cmd_queue[g_cmd_head];
    g_cmd_head = (g_cmd_head + 1) % CMD_QUEUE_SIZE;
    unlock_mutex();
    return true;
}

bool worker_enqueue_command(WorkerCommand cmd) {
    lock_mutex();
    int next_tail = (g_cmd_tail + 1) % CMD_QUEUE_SIZE;
    if (next_tail == g_cmd_head) {
        /* Queue full, drop */
        unlock_mutex();
        return false;
    }
    g_cmd_queue[g_cmd_tail] = cmd;
    g_cmd_tail = next_tail;
    unlock_mutex();
    return true;
}

static void handle_command(WorkerCommand cmd, const char *token) {
    if (!token || strlen(token) == 0) return;

    switch (cmd) {
        case CMD_TOGGLE_PLAY_PAUSE:
            if (g_playback_state.is_playing) {
                spotify_pause(token);
            } else {
                spotify_play(token);
            }
            break;
        case CMD_SKIP_NEXT:
            spotify_next(token);
            break;
        case CMD_SKIP_PREV:
            spotify_previous(token);
            break;
        case CMD_VOLUME_UP:
            spotify_set_volume(token, g_playback_state.volume_percent + 5);
            break;
        case CMD_VOLUME_DOWN:
            spotify_set_volume(token, g_playback_state.volume_percent - 5);
            break;
        case CMD_TOGGLE_SHUFFLE:
            spotify_set_shuffle(token, !g_playback_state.shuffle_state);
            break;
        case CMD_CYCLE_REPEAT: {
            SpotifyRepeatMode next_mode = (g_playback_state.repeat_state + 1) % 3;
            spotify_set_repeat(token, next_mode);
            break;
        }
        case CMD_FORCE_REFRESH:
        default:
            break;
    }
}

#if defined(__psp2__) || defined(__VITA__)
static int worker_thread_func(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
#else
static void* worker_thread_func(void *argp)
{
    (void)argp;
#endif

    uint64_t last_poll_tick = 0;
    static char s_logged_track[SPOTIFY_TRACK_NAME_MAX] = {0};

    LOG_INFO("Background worker thread started");

    while (g_running) {
        uint64_t now = get_time_ms();

#if defined(__psp2__) || defined(__VITA__)
        /* Check Wi-Fi state */
        int net_state = 0;
        int ctl_res = sceNetCtlInetGetState(&net_state);
        LOG_INFO("Worker Wi-Fi status check: result=%d, state=%d", ctl_res, net_state);
        if (ctl_res >= 0 && net_state != SCE_NETCTL_STATE_CONNECTED) {
            error_set(APP_ERR_WIFI_DISCONNECTED, "Wi-Fi Disconnected",
                      "PS Vita is not connected to a Wi-Fi network. Please check Vita Settings.",
                      "Press [X] to dismiss");
            sleep_ms(2000);
            continue;
        }
#endif

        /* Check if access token needs initial fetch or refresh */
        if (!g_authenticated || now >= g_token_expiry_tick) {
            g_syncing = true;
            char new_token[512] = {0};
            int expires_in = 3600;

            LOG_INFO("Worker: calling spotify_refresh_token (client_id='%.8s...', token_len=%u)",
                     g_config.client_id, (unsigned int)strlen(g_config.refresh_token));
            if (spotify_refresh_token(g_config.client_id, g_config.client_secret,
                                     g_config.refresh_token, new_token,
                                     sizeof(new_token), &expires_in)) {
                lock_mutex();
                utils_safe_strncpy(g_access_token, new_token, sizeof(g_access_token));
                /* Refresh 5 minutes before actual expiration */
                g_token_expiry_tick = now + ((expires_in > 300 ? expires_in - 300 : expires_in) * 1000);
                g_authenticated = true;
                unlock_mutex();

                if (error_is_active()) {
                    AppError err;
                    error_get(&err);
                    if (err.code == APP_ERR_SPOTIFY_AUTH || err.code == APP_ERR_SPOTIFY_TIMEOUT || err.code == APP_ERR_WIFI_DISCONNECTED) {
                        error_clear();
                    }
                }

                /* Immediately poll playback state right after authentication */
                last_poll_tick = 0;
            } else {
                lock_mutex();
                g_playback_state.auth_error = true;
                unlock_mutex();
                sleep_ms(3000);
                continue;
            }
            g_syncing = false;
        }

        /* Check for pending UI commands */
        WorkerCommand cmd;
        if (pop_command(&cmd)) {
            g_syncing = true;
            char token_copy[512];
            lock_mutex();
            strncpy(token_copy, g_access_token, sizeof(token_copy));
            unlock_mutex();

            LOG_INFO("Processing transport command: %d", (int)cmd);
            handle_command(cmd, token_copy);

            /* Delay 250ms to allow Spotify Web API target to apply command, then poll immediately */
            sleep_ms(250);
            SpotifyPlaybackState new_state;
            lock_mutex();
            new_state = g_playback_state;
            unlock_mutex();

            if (spotify_get_playback(token_copy, &new_state)) {
                lock_mutex();
                g_playback_state = new_state;
                g_state_updated_tick = get_time_ms();
                unlock_mutex();

                if (error_is_active()) {
                    AppError err;
                    error_get(&err);
                    if (err.code == APP_ERR_SPOTIFY_TIMEOUT || err.code == APP_ERR_SPOTIFY_RATE_LIMIT || err.code == APP_ERR_WIFI_DISCONNECTED || err.code == APP_ERR_SPOTIFY_NO_DEVICE) {
                        error_clear();
                    }
                }
            }
            last_poll_tick = get_time_ms();
            g_syncing = false;
            continue;
        }

        /* Regular Polling Loop (every 1.5 seconds) */
        if (now - last_poll_tick >= POLL_INTERVAL_MS) {
            char token_copy[512];
            lock_mutex();
            strncpy(token_copy, g_access_token, sizeof(token_copy));
            unlock_mutex();

            if (strlen(token_copy) > 0) {
                g_syncing = true;
                SpotifyPlaybackState new_state;
                lock_mutex();
                new_state = g_playback_state;
                unlock_mutex();

                if (spotify_get_playback(token_copy, &new_state)) {
                    lock_mutex();
                    g_playback_state = new_state;
                    g_state_updated_tick = get_time_ms();
                    unlock_mutex();

                    /* Clear transient errors if poll succeeded */
                    if (error_is_active()) {
                        AppError err;
                        error_get(&err);
                        if (err.code == APP_ERR_SPOTIFY_TIMEOUT || err.code == APP_ERR_SPOTIFY_RATE_LIMIT || err.code == APP_ERR_WIFI_DISCONNECTED || err.code == APP_ERR_SPOTIFY_NO_DEVICE) {
                            error_clear();
                        }
                    }

                    /* Log track changes */
                    if (strcmp(new_state.track_name, s_logged_track) != 0 && strlen(new_state.track_name) > 0) {
                        utils_safe_strncpy(s_logged_track, new_state.track_name, sizeof(s_logged_track));
                        LOG_INFO("Now playing: \"%s\" by %s [album: %s, device: %s, %s]",
                                 new_state.track_name, new_state.artist_name, new_state.album_name,
                                 new_state.device_name, new_state.is_playing ? "playing" : "paused");
                    }
                }
                g_syncing = false;
            }
            last_poll_tick = get_time_ms();
        }

        /* Sleep a bit before checking command queue again */
        sleep_ms(30);
    }

    LOG_INFO("Worker thread loop terminated");
#if defined(__psp2__) || defined(__VITA__)
    return sceKernelExitDeleteThread(0);
#else
    return NULL;
#endif
}

bool worker_start(const AppConfig *config) {
    if (!config) return false;
    g_config = *config;

    memset(&g_playback_state, 0, sizeof(SpotifyPlaybackState));
    g_playback_state.volume_percent = 50;

    g_running = true;
    g_authenticated = false;

    LOG_INFO("Starting background worker thread...");

#if defined(__psp2__) || defined(__VITA__)
    g_mutex = sceKernelCreateMutex("psvitaman_worker_mtx", 0, 0, NULL);
    if (g_mutex < 0) return false;

    g_thid = sceKernelCreateThread("psvitaman_worker", worker_thread_func, 0x10000100, 0x20000, 0, 0, NULL);
    if (g_thid < 0) return false;

    if (sceKernelStartThread(g_thid, 0, NULL) < 0) return false;
#else
    pthread_mutex_init(&g_mutex, NULL);
    if (pthread_create(&g_thread, NULL, worker_thread_func, NULL) != 0)
        return false;
#endif

    return true;
}

void worker_stop(void) {
    LOG_INFO("Stopping background worker thread...");
    g_running = false;

#if defined(__psp2__) || defined(__VITA__)
    if (g_thid >= 0) {
        sceKernelWaitThreadEnd(g_thid, NULL, NULL);
        g_thid = -1;
    }
    if (g_mutex >= 0) {
        sceKernelDeleteMutex(g_mutex);
        g_mutex = -1;
    }
#else
    pthread_join(g_thread, NULL);
    pthread_mutex_destroy(&g_mutex);
#endif
    LOG_INFO("Background worker stopped successfully");
}

void worker_get_playback_state(SpotifyPlaybackState *out_state, int *out_interpolated_progress_ms) {
    if (!out_state) return;

    lock_mutex();
    *out_state = g_playback_state;
    uint64_t updated_tick = g_state_updated_tick;
    unlock_mutex();

    if (out_interpolated_progress_ms) {
        if (out_state->is_playing && updated_tick > 0) {
            uint64_t now = get_time_ms();
            int elapsed = (int)(now - updated_tick);
            int current = out_state->progress_ms + elapsed;
            if (out_state->duration_ms > 0 && current > out_state->duration_ms) {
                current = out_state->duration_ms;
            }
            *out_interpolated_progress_ms = current;
        } else {
            *out_interpolated_progress_ms = out_state->progress_ms;
        }
    }
}

bool worker_is_syncing(void) {
    return g_syncing;
}

bool worker_is_authenticated(void) {
    return g_authenticated;
}
