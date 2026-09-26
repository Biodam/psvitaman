/**
 * PSVitaman - Mechanical Sound Effects Subsystem Implementation
 */

#include "sound.h"
#include "sound_data.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/kernel/threadmgr.h>
#include <psp2/audioout.h>

#define AUDIO_BUF_SAMPLES 512

static int s_audio_port = -1;
static SceUID s_audio_thread_uid = -1;
static volatile bool s_audio_running = false;
static volatile SoundEffect s_active_sound = SOUND_NONE;
static volatile int s_sound_sample_idx = 0;

static int audio_thread_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    int16_t out_buf[AUDIO_BUF_SAMPLES * 2]; /* Stereo interleaved: L, R */

    while (s_audio_running) {
        SoundEffect cur_sound = s_active_sound;
        if (cur_sound != SOUND_NONE) {
            const int16_t *src_pcm = NULL;
            int total_samples = 0;

            if (cur_sound == SOUND_CLACK) {
                src_pcm = s_pcm_clack;
                total_samples = SOUND_CLACK_SAMPLE_COUNT;
            } else if (cur_sound == SOUND_CLICK) {
                src_pcm = s_pcm_click;
                total_samples = SOUND_CLICK_SAMPLE_COUNT;
            }

            int cur_idx = s_sound_sample_idx;
            if (src_pcm && cur_idx < total_samples) {
                int samples_to_copy = total_samples - cur_idx;
                if (samples_to_copy > AUDIO_BUF_SAMPLES) {
                    samples_to_copy = AUDIO_BUF_SAMPLES;
                }

                for (int i = 0; i < samples_to_copy; i++) {
                    int16_t sample = src_pcm[cur_idx + i];
                    out_buf[i * 2]     = sample; /* Left */
                    out_buf[i * 2 + 1] = sample; /* Right */
                }

                /* Pad remaining buffer space with silence */
                for (int i = samples_to_copy; i < AUDIO_BUF_SAMPLES; i++) {
                    out_buf[i * 2]     = 0;
                    out_buf[i * 2 + 1] = 0;
                }

                cur_idx += samples_to_copy;
                s_sound_sample_idx = cur_idx;

                if (cur_idx >= total_samples) {
                    s_active_sound = SOUND_NONE;
                    s_sound_sample_idx = 0;
                }

                if (s_audio_port >= 0) {
                    sceAudioOutOutput(s_audio_port, out_buf);
                }
            } else {
                s_active_sound = SOUND_NONE;
                s_sound_sample_idx = 0;
                sceKernelDelayThread(8000); /* 8ms idle sleep */
            }
        } else {
            sceKernelDelayThread(8000); /* 8ms idle sleep */
        }
    }

    return 0;
}
#endif

void sound_init(void) {
#if defined(__psp2__) || defined(__VITA__)
    /* Open main stereo audio output port */
    s_audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_BUF_SAMPLES, 44100, SCE_AUDIO_OUT_MODE_STEREO);
    if (s_audio_port < 0) {
        LOG_WARN("Could not open MAIN audio port (%d), trying BGM port...", s_audio_port);
        s_audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, AUDIO_BUF_SAMPLES, 44100, SCE_AUDIO_OUT_MODE_STEREO);
    }

    if (s_audio_port >= 0) {
        /* Set max volume for tactile clicks */
        int vol[2] = { SCE_AUDIO_OUT_MAX_VOL, SCE_AUDIO_OUT_MAX_VOL };
        sceAudioOutSetVolume(s_audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);
        LOG_INFO("Audio port opened successfully (port: %d, rate: 44.1kHz stereo)", s_audio_port);

        s_audio_running = true;
        s_active_sound = SOUND_NONE;
        s_sound_sample_idx = 0;

        s_audio_thread_uid = sceKernelCreateThread("psvitaman_audio", audio_thread_entry,
                                                  0x10000100 + 10, /* Priority slightly below render thread */
                                                  64 * 1024,       /* 64 KB Stack */
                                                  0, 0, NULL);
        if (s_audio_thread_uid >= 0) {
            sceKernelStartThread(s_audio_thread_uid, 0, NULL);
            LOG_INFO("Mechanical audio thread started (UID: 0x%08X)", s_audio_thread_uid);
        } else {
            LOG_WARN("Failed to create audio thread: 0x%08X", s_audio_thread_uid);
        }
    } else {
        LOG_WARN("Failed to open audio port: %d", s_audio_port);
    }
#else
    LOG_INFO("Sound subsystem initialized (mock)");
#endif
}

void sound_cleanup(void) {
#if defined(__psp2__) || defined(__VITA__)
    s_audio_running = false;
    if (s_audio_thread_uid >= 0) {
        sceKernelWaitThreadEnd(s_audio_thread_uid, NULL, NULL);
        sceKernelDeleteThread(s_audio_thread_uid);
        s_audio_thread_uid = -1;
    }
    if (s_audio_port >= 0) {
        sceAudioOutReleasePort(s_audio_port);
        s_audio_port = -1;
    }
    LOG_INFO("Sound subsystem cleaned up");
#endif
}

void sound_play(SoundEffect effect) {
    if (effect == SOUND_NONE) return;

#if defined(__psp2__) || defined(__VITA__)
    s_sound_sample_idx = 0;
    s_active_sound = effect;
#endif
}
