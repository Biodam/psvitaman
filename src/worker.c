/**
 * PSVitaman - Background Worker & Concurrency Engine Implementation
 */

#include "worker.h"
#include "spotify.h"
#include "utils.h"
#include "logger.h"
#include "error.h"
#include "try_catch.h"
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
typedef struct {
    WorkerCommand type;
    int int_val;
} WorkerQueueItem;

static WorkerQueueItem g_cmd_queue[CMD_QUEUE_SIZE];
static int g_cmd_head = 0;
static int g_cmd_tail = 0;

static uint64_t s_play_override_tick = 0;
static bool s_play_override_val = false;

static uint64_t s_shuffle_override_tick = 0;
static bool s_shuffle_override_val = false;

static uint64_t s_repeat_override_tick = 0;
static SpotifyRepeatMode s_repeat_override_val = REPEAT_OFF;

static uint64_t s_volume_override_tick = 0;
static int s_volume_override_val = 50;

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

static bool pop_command(WorkerQueueItem *item) {
    lock_mutex();
    if (g_cmd_head == g_cmd_tail) {
        unlock_mutex();
        return false;
    }
    *item = g_cmd_queue[g_cmd_head];
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

    uint64_t now = get_time_ms();
    WorkerQueueItem item;
    item.type = cmd;
    item.int_val = 0;

    /* Immediate optimistic UI state mutation - zero latency response */
    switch (cmd) {
        case CMD_PLAY:
            g_playback_state.is_playing = true;
            s_play_override_val = true;
            s_play_override_tick = now;
            g_state_updated_tick = now;
            item.int_val = 1;
            break;

        case CMD_PAUSE:
            g_playback_state.is_playing = false;
            s_play_override_val = false;
            s_play_override_tick = now;
            g_state_updated_tick = now;
            item.int_val = 0;
            break;

        case CMD_TOGGLE_PLAY_PAUSE: {
            bool target = !g_playback_state.is_playing;
            g_playback_state.is_playing = target;
            s_play_override_val = target;
            s_play_override_tick = now;
            g_state_updated_tick = now;
            item.type = target ? CMD_PLAY : CMD_PAUSE;
            item.int_val = target ? 1 : 0;
            break;
        }

        case CMD_TOGGLE_SHUFFLE: {
            bool target = !g_playback_state.shuffle_state;
            g_playback_state.shuffle_state = target;
            s_shuffle_override_val = target;
            s_shuffle_override_tick = now;
            item.int_val = target ? 1 : 0;
            LOG_INFO("Worker (optimistic): shuffle toggled to %s", target ? "ON" : "OFF");
            break;
        }

        case CMD_CYCLE_REPEAT: {
            SpotifyRepeatMode next_mode = (g_playback_state.repeat_state + 1) % 3;
            g_playback_state.repeat_state = next_mode;
            s_repeat_override_val = next_mode;
            s_repeat_override_tick = now;
            item.int_val = (int)next_mode;
            LOG_INFO("Worker (optimistic): repeat cycled to mode %d", (int)next_mode);
            break;
        }

        case CMD_VOLUME_UP: {
            int v = g_playback_state.volume_percent + 5;
            if (v > 100) v = 100;
            unlock_mutex();
            return worker_set_volume(v);
        }

        case CMD_VOLUME_DOWN: {
            int v = g_playback_state.volume_percent - 5;
            if (v < 0) v = 0;
            unlock_mutex();
            return worker_set_volume(v);
        }

        case CMD_SKIP_NEXT:
        case CMD_SKIP_PREV:
        case CMD_FORCE_REFRESH:
        default:
            break;
    }

    g_cmd_queue[g_cmd_tail] = item;
    g_cmd_tail = next_tail;
    unlock_mutex();
    return true;
}

bool worker_set_volume(int volume_percent) {
    if (volume_percent < 0) volume_percent = 0;
    if (volume_percent > 100) volume_percent = 100;

    lock_mutex();
    uint64_t now = get_time_ms();

    /* Immediately mutate optimistic state */
    g_playback_state.volume_percent = volume_percent;
    s_volume_override_val = volume_percent;
    s_volume_override_tick = now;

    /* Check if the most recent unconsumed item in the queue is already a volume command */
    if (g_cmd_head != g_cmd_tail) {
        int prev_idx = (g_cmd_tail - 1 + CMD_QUEUE_SIZE) % CMD_QUEUE_SIZE;
        if (g_cmd_queue[prev_idx].type == CMD_SET_VOLUME ||
            g_cmd_queue[prev_idx].type == CMD_VOLUME_UP ||
            g_cmd_queue[prev_idx].type == CMD_VOLUME_DOWN) {
            /* Coalesce: update target volume in place without spamming redundant HTTP requests */
            g_cmd_queue[prev_idx].type = CMD_SET_VOLUME;
            g_cmd_queue[prev_idx].int_val = volume_percent;
            unlock_mutex();
            return true;
        }
    }

    int next_tail = (g_cmd_tail + 1) % CMD_QUEUE_SIZE;
    if (next_tail == g_cmd_head) {
        unlock_mutex();
        return false;
    }

    WorkerQueueItem item;
    item.type = CMD_SET_VOLUME;
    item.int_val = volume_percent;

    g_cmd_queue[g_cmd_tail] = item;
    g_cmd_tail = next_tail;
    unlock_mutex();
    return true;
}

static void handle_command(WorkerQueueItem item, const char *token) {
    if (!token || strlen(token) == 0) return;

    switch (item.type) {
        case CMD_PLAY:
            if (!spotify_play(token)) {
                LOG_WARN("Worker: spotify_play failed! Reverting optimistic state");
                lock_mutex();
                s_play_override_tick = 0;
                g_playback_state.is_playing = false;
                unlock_mutex();
            }
            break;

        case CMD_PAUSE:
            if (!spotify_pause(token)) {
                LOG_WARN("Worker: spotify_pause failed! Reverting optimistic state");
                lock_mutex();
                s_play_override_tick = 0;
                g_playback_state.is_playing = true;
                unlock_mutex();
            }
            break;

        case CMD_SKIP_NEXT:
            spotify_next(token);
            break;

        case CMD_SKIP_PREV:
            spotify_previous(token);
            break;

        case CMD_VOLUME_UP:
        case CMD_VOLUME_DOWN:
        case CMD_SET_VOLUME:
            spotify_set_volume(token, item.int_val);
            break;

        case CMD_TOGGLE_SHUFFLE: {
            bool target = (item.int_val != 0);
            LOG_INFO("Worker: executing spotify_set_shuffle (target=%s)...", target ? "true" : "false");
            if (spotify_set_shuffle(token, target)) {
                LOG_INFO("Worker: spotify_set_shuffle succeeded for %s", target ? "ON" : "OFF");
            } else {
                LOG_WARN("Worker: spotify_set_shuffle failed, reverting optimistic state");
                lock_mutex();
                if (s_shuffle_override_val == target) {
                    s_shuffle_override_tick = 0;
                    g_playback_state.shuffle_state = !target;
                    s_shuffle_override_val = !target;
                }
                unlock_mutex();
            }
            break;
        }

        case CMD_CYCLE_REPEAT: {
            SpotifyRepeatMode target = (SpotifyRepeatMode)item.int_val;
            LOG_INFO("Worker: executing spotify_set_repeat (mode=%d)...", (int)target);
            if (spotify_set_repeat(token, target)) {
                LOG_INFO("Worker: spotify_set_repeat succeeded for mode %d", (int)target);
            } else {
                LOG_WARN("Worker: spotify_set_repeat failed, reverting optimistic state");
                lock_mutex();
                if (s_repeat_override_val == target) {
                    s_repeat_override_tick = 0;
                    SpotifyRepeatMode prev = (target + 2) % 3;
                    g_playback_state.repeat_state = prev;
                    s_repeat_override_val = prev;
                }
                unlock_mutex();
            }
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
        ExceptionFrame frame;
        TRY(frame) {
            uint64_t now = get_time_ms();

#if defined(__psp2__) || defined(__VITA__)
            /* Check Wi-Fi state every 3 seconds without spamming logs */
            static uint64_t s_last_wifi_check_tick = 0;
            static int s_last_net_state = SCE_NETCTL_STATE_CONNECTED;
            if (now - s_last_wifi_check_tick >= 3000) {
                s_last_wifi_check_tick = now;
                int net_state = 0;
                int ctl_res = sceNetCtlInetGetState(&net_state);
                if (ctl_res >= 0 && net_state != s_last_net_state) {
                    LOG_INFO("Wi-Fi connection state changed: %d -> %d", s_last_net_state, net_state);
                    s_last_net_state = net_state;
                }
                if (ctl_res >= 0 && net_state != SCE_NETCTL_STATE_CONNECTED) {
                    error_set(APP_ERR_WIFI_DISCONNECTED, "Wi-Fi Disconnected",
                              "PS Vita is not connected to a Wi-Fi network. Please check Vita Settings.",
                              "Press [X] to dismiss");
                    sleep_ms(2000);
                    continue;
                }
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
                    sleep_ms(15000);
                    continue;
                }
                g_syncing = false;
            }

            /* Check for pending UI commands */
            WorkerQueueItem item;
            if (pop_command(&item)) {
                g_syncing = true;
                char token_copy[512];
                lock_mutex();
                strncpy(token_copy, g_access_token, sizeof(token_copy));
                unlock_mutex();

                LOG_INFO("Processing transport command: %d", (int)item.type);
                handle_command(item, token_copy);

                /* Delay 250ms to allow Spotify Web API target to apply command, then poll immediately */
                sleep_ms(250);
                SpotifyPlaybackState new_state;
                lock_mutex();
                new_state = g_playback_state;
                unlock_mutex();

                if (spotify_get_playback(token_copy, &new_state)) {
                    uint64_t cur_t = get_time_ms();
                    lock_mutex();
                    /* Grace window (2000ms): protect optimistic states while Spotify backend synchronizes */
                    if (cur_t - s_play_override_tick < 2000) {
                        new_state.is_playing = s_play_override_val;
                    }
                    if (cur_t - s_shuffle_override_tick < 2000) {
                        new_state.shuffle_state = s_shuffle_override_val;
                    }
                    if (cur_t - s_repeat_override_tick < 2000) {
                        new_state.repeat_state = s_repeat_override_val;
                    }
                    if (cur_t - s_volume_override_tick < 2000) {
                        new_state.volume_percent = s_volume_override_val;
                    }
                    g_playback_state = new_state;
                    g_state_updated_tick = cur_t;
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
                        uint64_t cur_t = get_time_ms();
                        lock_mutex();
                        /* Grace window (2000ms): protect optimistic states while Spotify backend synchronizes */
                        if (cur_t - s_play_override_tick < 2000) {
                            new_state.is_playing = s_play_override_val;
                        }
                        if (cur_t - s_shuffle_override_tick < 2000) {
                            new_state.shuffle_state = s_shuffle_override_val;
                        }
                        if (cur_t - s_repeat_override_tick < 2000) {
                            new_state.repeat_state = s_repeat_override_val;
                        }
                        if (cur_t - s_volume_override_tick < 2000) {
                            new_state.volume_percent = s_volume_override_val;
                        }
                        g_playback_state = new_state;
                        g_state_updated_tick = cur_t;
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
        } CATCH(frame) {
            LOG_ERROR("Caught exception in worker thread: %s (code 0x%04x) at %s:%d",
                      frame.message, (unsigned int)frame.code, frame.file, frame.line);
            g_syncing = false;
            error_set(APP_ERR_GENERIC_EXCEPTION, "Worker Exception",
                      frame.message, "Press [X] to dismiss");
            sleep_ms(2000);
        }
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
    if (g_running) {
        worker_stop();
    }

    g_config = *config;

    memset(&g_playback_state, 0, sizeof(SpotifyPlaybackState));
    g_playback_state.volume_percent = 50;
    s_play_override_tick = 0;
    s_shuffle_override_tick = 0;
    s_repeat_override_tick = 0;
    s_volume_override_tick = 0;

    g_running = true;
    g_authenticated = false;

    LOG_INFO("Starting background worker thread...");

#if defined(__psp2__) || defined(__VITA__)
    g_mutex = sceKernelCreateMutex("psvitaman_worker_mtx", 0, 0, NULL);
    if (g_mutex < 0) {
        LOG_ERROR("worker_start: sceKernelCreateMutex failed: 0x%08X", (unsigned int)g_mutex);
        g_running = false;
        return false;
    }

    g_thid = sceKernelCreateThread("psvitaman_worker", worker_thread_func, 0x10000100, 0x20000, 0, 0, NULL);
    if (g_thid < 0) {
        LOG_ERROR("worker_start: sceKernelCreateThread failed: 0x%08X", (unsigned int)g_thid);
        sceKernelDeleteMutex(g_mutex);
        g_mutex = -1;
        g_running = false;
        return false;
    }

    int start_res = sceKernelStartThread(g_thid, 0, NULL);
    if (start_res < 0) {
        LOG_ERROR("worker_start: sceKernelStartThread failed: 0x%08X", (unsigned int)start_res);
        sceKernelDeleteThread(g_thid);
        g_thid = -1;
        sceKernelDeleteMutex(g_mutex);
        g_mutex = -1;
        g_running = false;
        return false;
    }
#else
    pthread_mutex_init(&g_mutex, NULL);
    if (pthread_create(&g_thread, NULL, worker_thread_func, NULL) != 0) {
        g_running = false;
        return false;
    }
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

bool worker_is_running(void) {
    return g_running;
}

