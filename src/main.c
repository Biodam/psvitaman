/**
 * PSVitaman - Spotify Remote for PlayStation Vita
 * Main Entry Point
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "config.h"
#include "spotify.h"
#include "worker.h"
#include "input.h"
#include "ui.h"
#include "http_server.h"
#include "logger.h"
#include "error.h"
#include "root_certs.h"

#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "dev"
#endif

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/libssl.h>
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
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
    sceSysmoduleLoadModule(SCE_SYSMODULE_SSL);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS);
    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);

    /* 2. Initialize SceNet Stack */
    SceNetInitParam net_param;
    net_param.memory = s_net_memory;
    net_param.size = sizeof(s_net_memory);
    net_param.flags = 0;
    sceNetInit(&net_param);
    sceNetCtlInit();

    /* 3. Initialize SceSsl & SceHttp Native Stacks with Embedded Root CAs */
    sceSslInit(1024 * 1024);
    sceHttpInit(1024 * 1024);
    root_certs_load();
    sceHttpsEnableOption(
        SCE_HTTPS_FLAG_SERVER_VERIFY |
        SCE_HTTPS_FLAG_CN_CHECK |
        SCE_HTTPS_FLAG_KNOWN_CA_CHECK
    );

    /* 4. Initialize vita2d Graphics */
    vita2d_init();
    vita2d_set_clear_color(RGBA8(22, 26, 34, 255));
#endif

    /* 5. Initialize Network & Diagnostics */
    log_init("ux0:data/psvitaman/psvitaman.log");
    error_init();
    LOG_INFO("PSVitaman initializing... [commit: %s]", GIT_COMMIT_HASH);

    spotify_init();
    input_init();
    ui_init();

    /* 5. Check Network & Load Configuration */
    char local_ip[32] = {0};
    if (http_server_get_local_ip(local_ip, sizeof(local_ip))) {
        LOG_INFO("Wi-Fi network active - Vita IP: %s", local_ip);
    } else {
        LOG_WARN("Wi-Fi not connected or IP not assigned at startup");
    }

    AppConfig config;
    config_load(&config);

    if (config.is_valid) {
        LOG_INFO("Loaded valid configuration - starting Spotify worker");
        worker_start(&config);
    } else {
        LOG_INFO("Setup required - starting HTTP pairing server on port %d", HTTP_SERVER_DEFAULT_PORT);
        http_server_start(HTTP_SERVER_DEFAULT_PORT, &config);
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

        /* Retrieve snapshot of active error modal if present */
        AppError current_error = {0};
        bool has_error = error_is_active();
        if (has_error) {
            error_get(&current_error);
        }

#if defined(__psp2__) || defined(__VITA__)
        /* Handle active error modal dismissal & shortcuts */
        if (has_error) {
            if (input.pressed_buttons & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE)) {
                error_clear();
                has_error = false;
            } else if (current_error.code == APP_ERR_SPOTIFY_AUTH && (input.pressed_buttons & SCE_CTRL_SELECT)) {
                error_clear();
                has_error = false;
                show_qr_overlay = true;
                if (!http_server_is_running()) {
                    http_server_start(HTTP_SERVER_DEFAULT_PORT, &config);
                }
            }
        }
#endif

        if (config.is_valid) {
#if defined(__psp2__) || defined(__VITA__)
            /* Toggle QR Code pairing overlay with SELECT */
            if (!has_error && (input.pressed_buttons & SCE_CTRL_SELECT)) {
                show_qr_overlay = !show_qr_overlay;
                if (show_qr_overlay) {
                    if (!http_server_is_running()) http_server_start(HTTP_SERVER_DEFAULT_PORT, &config);
                } else {
                    if (http_server_is_running()) http_server_stop();
                }
            }
            if (show_qr_overlay && (input.pressed_buttons & SCE_CTRL_CIRCLE)) {
                show_qr_overlay = false;
                if (http_server_is_running()) http_server_stop();
            }

            /* Only process playback buttons if modal overlay and error are not blocking */
            if (!show_qr_overlay && !has_error) {
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
            /* Check if HTTP pairing server received auth credentials from phone */
            if (http_server_has_received_auth()) {
                LOG_INFO("Auth received! Stopping HTTP server and starting worker");
                http_server_stop();
                show_qr_overlay = false;
                error_clear();
                if (config_load(&config) && config.is_valid) {
                    worker_start(&config);
                }
            }

            /* If in Setup Mode, allow pressing START to reload config */
#if defined(__psp2__) || defined(__VITA__)
            if (input.pressed_buttons & SCE_CTRL_START) {
                if (config_load(&config) && config.is_valid) {
                    http_server_stop();
                    worker_start(&config);
                }
            }
#endif
        }

        /* Also check if QR overlay in paired mode received new token */
        if (show_qr_overlay && http_server_has_received_auth()) {
            LOG_INFO("Re-pairing credentials received! Refreshing worker");
            http_server_stop();
            show_qr_overlay = false;
            error_clear();
            if (config_load(&config) && config.is_valid) {
                worker_stop();
                worker_start(&config);
            }
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
        ui_render(&playback, interpolated_progress_ms, &input, &config,
                  worker_is_syncing(), show_qr_overlay, &current_error);
    }

    /* Shutdown Sequence */
    LOG_INFO("PSVitaman shutting down...");
    if (http_server_is_running()) {
        http_server_stop();
    }
    if (config.is_valid) {
        worker_stop();
    }

    ui_cleanup();
    spotify_cleanup();
    error_cleanup();
    log_close();

#if defined(__psp2__) || defined(__VITA__)
    vita2d_fini();
    root_certs_unload();
    sceHttpTerm();
    sceSslTerm();
    sceNetCtlTerm();
    sceNetTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_SSL);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_PGF);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    sceKernelExitProcess(0);
#endif

    return 0;
}
