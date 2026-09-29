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
#include "sound.h"

#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "dev"
#endif

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/ctrl.h>
#include <psp2/avconfig.h>
#include <vita2d.h>

#define NET_INIT_SIZE (1 * 1024 * 1024)
static char s_net_memory[NET_INIT_SIZE];
static int s_last_sys_vol = -1;
static int s_last_applied_spot_vol = -1;
static uint64_t s_vol_sync_ignore_tick = 0;
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* 1. Initialize Logger & Diagnostics immediately */
    log_init("ux0:data/psvitaman/psvitaman.log");
    error_init();
    LOG_INFO("==================================================");
    LOG_INFO(" PSVitaman initializing... [commit: %s]", GIT_COMMIT_HASH);
    LOG_INFO("==================================================");

#if defined(__psp2__) || defined(__VITA__)
    /* 2. Load System Modules */
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVCONFIG);

    /* 3. Initialize SceNet Stack */
    SceNetInitParam net_param;
    net_param.memory = s_net_memory;
    net_param.size = sizeof(s_net_memory);
    net_param.flags = 0;
    sceNetInit(&net_param);
    sceNetCtlInit();

    /* 4. Initialize vita2d Graphics with 8 MB temporary draw pool */
    vita2d_init_advanced(8 * 1024 * 1024);
    vita2d_set_clear_color(RGBA8(22, 26, 34, 255));
#endif

    spotify_init();
    input_init();
    ui_init();
    sound_init();

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

    /* Apply user's saved theme from config */
    ui_set_theme(config.theme);

#if defined(__psp2__) || defined(__VITA__)
    uint64_t last_tick = sceKernelGetProcessTimeWide();
#endif

    bool app_running = true;
    bool show_qr_overlay = false;

    while (app_running) {
#if defined(__psp2__) || defined(__VITA__)
        /* Prevent screen dimming, OLED turn-off, and auto-suspend while running */
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF);
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING);
#endif

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
            } else if (input.pressed_buttons & SCE_CTRL_SELECT) {
                /* Pressing SELECT resets pairing: removes invalid config and switches cleanly to Setup Mode */
                LOG_INFO("SELECT pressed on error: resetting config and entering Setup Mode");
                error_clear();
                has_error = false;
                worker_stop();
                remove("ux0:data/psvitaman/config.ini");
                memset(&config, 0, sizeof(config));
                config.is_valid = false;
                show_qr_overlay = false;
                http_server_clear_auth_received();
                if (!http_server_is_running()) {
                    http_server_start(HTTP_SERVER_DEFAULT_PORT, &config);
                }
            }
        }
#endif

        /* Check if HTTP pairing server received auth credentials from phone (Setup Mode or QR Overlay) */
        if (http_server_has_received_auth()) {
            LOG_INFO("Auth received! Stopping HTTP server and starting worker");
            http_server_stop();
            show_qr_overlay = false;
            error_clear();
            if (config_load(&config) && config.is_valid) {
                worker_stop();
                worker_start(&config);
            }
        }

        if (config.is_valid) {
            /* Failsafe: ensure worker is running when config is valid */
            if (!worker_is_running()) {
                LOG_INFO("Config is valid but worker is not running - starting worker");
                worker_start(&config);
            }

#if defined(__psp2__) || defined(__VITA__)
            /* Pressing SELECT on the deck resets pairing and enters Setup Mode */
            if (!has_error && (input.pressed_buttons & SCE_CTRL_SELECT)) {
                LOG_INFO("SELECT pressed on deck: resetting config and entering Setup Mode");
                worker_stop();
                remove("ux0:data/psvitaman/config.ini");
                memset(&config, 0, sizeof(config));
                config.is_valid = false;
                show_qr_overlay = false;
                error_clear();
                http_server_clear_auth_received();
                if (!http_server_is_running()) {
                    http_server_start(HTTP_SERVER_DEFAULT_PORT, &config);
                }
            }
            if (show_qr_overlay && (input.pressed_buttons & SCE_CTRL_CIRCLE)) {
                show_qr_overlay = false;
                if (http_server_is_running()) http_server_stop();
                worker_start(&config);
            }

            /* Only process playback buttons if modal overlay and error are not blocking */
            if (!show_qr_overlay && !has_error) {
                if (input.pressed_buttons & SCE_CTRL_CROSS) {
                    worker_enqueue_command(CMD_PLAY);
                    sound_play(SOUND_CLACK);
                }
                if (input.pressed_buttons & SCE_CTRL_CIRCLE) {
                    worker_enqueue_command(CMD_PAUSE);
                    sound_play(SOUND_CLACK);
                }
                if (input.pressed_buttons & SCE_CTRL_RTRIGGER) {
                    worker_enqueue_command(CMD_SKIP_NEXT);
                    sound_play(SOUND_CLACK);
                }
                if (input.pressed_buttons & SCE_CTRL_LTRIGGER) {
                    worker_enqueue_command(CMD_SKIP_PREV);
                    sound_play(SOUND_CLACK);
                }
                if (input.pressed_buttons & SCE_CTRL_SQUARE) {
                    worker_enqueue_command(CMD_TOGGLE_SHUFFLE);
                    sound_play(SOUND_CLACK);
                }
                if (input.pressed_buttons & SCE_CTRL_TRIANGLE) {
                    worker_enqueue_command(CMD_CYCLE_REPEAT);
                    sound_play(SOUND_CLACK);
                }
                if (input.pressed_buttons & SCE_CTRL_UP) {
                    worker_enqueue_command(CMD_VOLUME_UP);
                    sound_play(SOUND_CLICK);
#if defined(__psp2__) || defined(__VITA__)
                    int next_spot = s_last_applied_spot_vol + 5;
                    if (next_spot > 100) next_spot = 100;
                    int v = (int)((next_spot * 30.0f / 100.0f) + 0.5f);
                    if (v > 30) v = 30;
                    sceAVConfigSetSystemVol(v);
                    s_last_sys_vol = v;
                    s_last_applied_spot_vol = next_spot;
                    s_vol_sync_ignore_tick = sceKernelGetProcessTimeWide() / 1000;
#endif
                }
                if (input.pressed_buttons & SCE_CTRL_DOWN) {
                    worker_enqueue_command(CMD_VOLUME_DOWN);
                    sound_play(SOUND_CLICK);
#if defined(__psp2__) || defined(__VITA__)
                    int next_spot = s_last_applied_spot_vol - 5;
                    if (next_spot < 0) next_spot = 0;
                    int v = (int)((next_spot * 30.0f / 100.0f) + 0.5f);
                    if (v < 0) v = 0;
                    sceAVConfigSetSystemVol(v);
                    s_last_sys_vol = v;
                    s_last_applied_spot_vol = next_spot;
                    s_vol_sync_ignore_tick = sceKernelGetProcessTimeWide() / 1000;
#endif
                }
                if (input.pressed_buttons & SCE_CTRL_START) {
                    worker_enqueue_command(CMD_FORCE_REFRESH);
                    sound_play(SOUND_CLICK);
                }
            }
#endif
        } else {
            /* In Setup Mode (config.is_valid == false) */
#if defined(__psp2__) || defined(__VITA__)
            /* Pressing SELECT in Setup Mode wipes config cleanly */
            if (input.pressed_buttons & SCE_CTRL_SELECT) {
                LOG_INFO("SELECT pressed in Setup Mode: wiping config");
                remove("ux0:data/psvitaman/config.ini");
                memset(&config, 0, sizeof(config));
                config.is_valid = false;
                sound_play(SOUND_CLACK);
                http_server_clear_auth_received();
            }

            /* If in Setup Mode, allow pressing START to reload config */
            if (input.pressed_buttons & SCE_CTRL_START) {
                sound_play(SOUND_CLICK);
                if (config_load(&config) && config.is_valid) {
                    http_server_stop();
                    worker_start(&config);
                }
            }
#endif
        }

#if defined(__psp2__) || defined(__VITA__)
        /* Theme Cycling (D-pad Left / Right or touch THEME button) */
        if (!show_qr_overlay && !has_error) {
            if (input.pressed_buttons & SCE_CTRL_RIGHT) {
                ui_cycle_theme();
                config.theme = ui_get_theme();
                sound_play(SOUND_CLICK);
                if (config.is_valid) {
                    config_save(&config);
                }
                LOG_INFO("Theme changed to: %s (%d)", ui_get_theme_name(config.theme), config.theme);
            } else if (input.pressed_buttons & SCE_CTRL_LEFT) {
                ui_cycle_theme_prev();
                config.theme = ui_get_theme();
                sound_play(SOUND_CLICK);
                if (config.is_valid) {
                    config_save(&config);
                }
                LOG_INFO("Theme changed to: %s (%d)", ui_get_theme_name(config.theme), config.theme);
            }
        }
#endif

        /* Retrieve snapshot of playback state */
        SpotifyPlaybackState playback;
        int interpolated_progress_ms = 0;
        if (config.is_valid) {
            worker_get_playback_state(&playback, &interpolated_progress_ms);
        } else {
            memset(&playback, 0, sizeof(playback));
        }

#if defined(__psp2__) || defined(__VITA__)
        /* Hardware Volume Key & Spotify Bidirectional Volume Sync */
        if (config.is_valid && worker_is_authenticated()) {
            uint64_t now_ms = sceKernelGetProcessTimeWide() / 1000;
            int current_sys_vol = -1;

            if (sceAVConfigGetSystemVol(&current_sys_vol) == 0 && current_sys_vol >= 0) {
                if (s_last_sys_vol < 0) {
                    /* Initial sync on startup / authentication */
                    s_last_sys_vol = current_sys_vol;
                    s_last_applied_spot_vol = playback.volume_percent;
                } else if (current_sys_vol != s_last_sys_vol) {
                    /* Direction 1: User pressed physical PS Vita Volume Up / Down hardware buttons! */
                    s_last_sys_vol = current_sys_vol;
                    int target_spot_vol = (int)((current_sys_vol * 100.0f / 30.0f) + 0.5f);
                    if (target_spot_vol < 0) target_spot_vol = 0;
                    if (target_spot_vol > 100) target_spot_vol = 100;

                    s_last_applied_spot_vol = target_spot_vol;
                    s_vol_sync_ignore_tick = now_ms;
                    worker_set_volume(target_spot_vol);
                    sound_play(SOUND_CLICK);
                    LOG_INFO("Hardware Volume button pressed: sys=%d/30 -> spotify=%d%%", current_sys_vol, target_spot_vol);
                } else if (now_ms - s_vol_sync_ignore_tick >= 2000 &&
                           playback.volume_percent >= 0 &&
                           playback.volume_percent != s_last_applied_spot_vol) {
                    /* Direction 2: Spotify volume changed externally (phone / desktop / connect) */
                    int target_sys_vol = (int)((playback.volume_percent * 30.0f / 100.0f) + 0.5f);
                    if (target_sys_vol < 0) target_sys_vol = 0;
                    if (target_sys_vol > 30) target_sys_vol = 30;

                    if (target_sys_vol != s_last_sys_vol) {
                        LOG_INFO("External Spotify volume changed: spotify=%d%% -> sys=%d/30", playback.volume_percent, target_sys_vol);
                        sceAVConfigSetSystemVol(target_sys_vol);
                        s_last_sys_vol = target_sys_vol;
                    }
                    s_last_applied_spot_vol = playback.volume_percent;
                }
            }
        }
#endif

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

    sound_cleanup();
    ui_cleanup();
    spotify_cleanup();
    error_cleanup();
    log_close();

#if defined(__psp2__) || defined(__VITA__)
    vita2d_fini();
    sceNetCtlTerm();
    sceNetTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_AVCONFIG);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_PGF);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    sceKernelExitProcess(0);
#endif

    return 0;
}
