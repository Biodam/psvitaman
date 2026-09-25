/**
 * PSVitaman - Spotify Remote for PlayStation Vita
 * Main Entry Point
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>

#include "config.h"
#include "spotify.h"
#include "worker.h"
#include "input.h"
#include "ui.h"

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/ctrl.h>
#include <vita2d.h>

#define NET_INIT_SIZE (1 * 1024 * 1024)
static char s_net_memory[NET_INIT_SIZE];
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

#if defined(__psp2__) || defined(__VITA__)
    /* 1. Load System Modules */
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);

    /* 2. Initialize SceNet Stack */
    SceNetInitParam net_param;
    net_param.memory = s_net_memory;
    net_param.size = sizeof(s_net_memory);
    net_param.flags = 0;
    sceNetInit(&net_param);
    sceNetCtlInit();

    /* 3. Initialize vita2d Graphics */
    vita2d_init();
    vita2d_set_clear_color(RGBA8(22, 26, 34, 255));
#endif

    /* 4. Initialize Network & Libraries */
    curl_global_init(CURL_GLOBAL_ALL);
    input_init();
    ui_init();

    /* 5. Load App Configuration */
    AppConfig config;
    config_load(&config);

    if (config.is_valid) {
        worker_start(&config);
    }

#if defined(__psp2__) || defined(__VITA__)
    uint64_t last_tick = sceKernelGetProcessTimeWide();
#endif

    bool app_running = true;
    bool show_qr_overlay = false;

    while (app_running) {
        /* Calculate Delta Time */
        float dt = 0.0166f; /* Default ~60fps */
#if defined(__psp2__) || defined(__VITA__)
        uint64_t current_tick = sceKernelGetProcessTimeWide();
        dt = (float)(current_tick - last_tick) / 1000000.0f;
        if (dt <= 0.0f || dt > 0.1f) dt = 0.0166f;
        last_tick = current_tick;
#endif

        /* Poll Gamepad & Touchscreen */
        InputState input;
        input_poll(&input);

        if (config.is_valid) {
#if defined(__psp2__) || defined(__VITA__)
            /* Toggle QR Code pairing overlay with SELECT */
            if (input.pressed_buttons & SCE_CTRL_SELECT) {
                show_qr_overlay = !show_qr_overlay;
            }
            if (show_qr_overlay && (input.pressed_buttons & SCE_CTRL_CIRCLE)) {
                show_qr_overlay = false;
            }

            /* Only process playback buttons if modal overlay is not blocking */
            if (!show_qr_overlay) {
                if (input.pressed_buttons & SCE_CTRL_CROSS) {
                    worker_enqueue_command(CMD_TOGGLE_PLAY_PAUSE);
                }
                if (input.pressed_buttons & SCE_CTRL_RTRIGGER) {
                    worker_enqueue_command(CMD_SKIP_NEXT);
                }
                if (input.pressed_buttons & SCE_CTRL_LTRIGGER) {
                    worker_enqueue_command(CMD_SKIP_PREV);
                }
                if (input.pressed_buttons & SCE_CTRL_SQUARE) {
                    worker_enqueue_command(CMD_TOGGLE_SHUFFLE);
                }
                if (input.pressed_buttons & SCE_CTRL_TRIANGLE) {
                    worker_enqueue_command(CMD_CYCLE_REPEAT);
                }
                if (input.pressed_buttons & SCE_CTRL_UP) {
                    worker_enqueue_command(CMD_VOLUME_UP);
                }
                if (input.pressed_buttons & SCE_CTRL_DOWN) {
                    worker_enqueue_command(CMD_VOLUME_DOWN);
                }
                if (input.pressed_buttons & SCE_CTRL_START) {
                    worker_enqueue_command(CMD_FORCE_REFRESH);
                }
            }
#endif
        } else {
            /* If in Setup Mode, allow pressing START to reload config */
#if defined(__psp2__) || defined(__VITA__)
            if (input.pressed_buttons & SCE_CTRL_START) {
                if (config_load(&config) && config.is_valid) {
                    worker_start(&config);
                }
            }
#endif
        }

        /* Retrieve snapshot of playback state */
        SpotifyPlaybackState playback;
        int interpolated_progress_ms = 0;
        if (config.is_valid) {
            worker_get_playback_state(&playback, &interpolated_progress_ms);
        } else {
            memset(&playback, 0, sizeof(playback));
        }

        /* Update animations & physics */
        ui_update(dt, &playback, interpolated_progress_ms);

        /* Render frame */
        ui_render(&playback, interpolated_progress_ms, &input, &config, worker_is_syncing(), show_qr_overlay);
    }

    /* Shutdown Sequence */
    if (config.is_valid) {
        worker_stop();
    }

    ui_cleanup();
    curl_global_cleanup();

#if defined(__psp2__) || defined(__VITA__)
    vita2d_fini();
    sceNetCtlTerm();
    sceNetTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_PGF);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    sceKernelExitProcess(0);
#endif

    return 0;
}
