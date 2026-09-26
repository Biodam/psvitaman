/**
 * PSVitaman - Mechanical Sound Effects Subsystem
 */

#ifndef PSVITAMAN_SOUND_H
#define PSVITAMAN_SOUND_H

typedef enum {
    SOUND_NONE = 0,
    SOUND_CLACK,  /* Heavy mechanical cassette lever latch (Play, Pause, Skip, etc.) */
    SOUND_CLICK   /* Crisp rotary / theme toggle detent click */
} SoundEffect;

/* Initialize audio hardware & background sound thread */
void sound_init(void);

/* Clean up audio port and stop sound thread */
void sound_cleanup(void);

/* Trigger sound playback (non-blocking, thread-safe, instant execution) */
void sound_play(SoundEffect effect);

#endif /* PSVITAMAN_SOUND_H */
