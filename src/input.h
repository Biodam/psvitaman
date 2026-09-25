/**
 * PSVitaman - Input Subsystem (Physical Controls + Capacitive Touch)
 */

#ifndef PSVITAMAN_INPUT_H
#define PSVITAMAN_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#define DECK_BTN_COUNT 5

typedef enum {
    BTN_INDEX_PREV = 0,
    BTN_INDEX_PLAY_PAUSE = 1,
    BTN_INDEX_NEXT = 2,
    BTN_INDEX_SHUFFLE = 3,
    BTN_INDEX_REPEAT = 4,
    BTN_INDEX_NONE = -1
} DeckButtonIndex;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    const char *label;
    const char *hotkey_hint;
} DeckButtonRect;

typedef struct {
    bool touch_active;
    int touch_x;
    int touch_y;
    int pressed_button;       /* Index 0..4 currently visually pressed, or -1 */
    uint32_t raw_buttons;     /* Currently held buttons */
    uint32_t pressed_buttons; /* Pressed on this frame (edge) */
} InputState;

/* Initialize input system */
void input_init(void);

/* Poll Vita gamepad and touchscreen */
void input_poll(InputState *state);

/* Return button boundary rectangles for UI rendering and hit tests */
const DeckButtonRect *input_get_button_rects(void);

#endif /* PSVITAMAN_INPUT_H */
