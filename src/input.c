/**
 * PSVitaman - Input Subsystem Implementation
 */

#include "input.h"
#include <string.h>

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#endif

static uint32_t s_prev_buttons = 0;
static bool s_prev_touch = false;
static int s_touch_held_btn = -1;

/* Deck Button Layout: 5 mechanical audio buttons across 960 width */
static const DeckButtonRect s_deck_buttons[DECK_BTN_COUNT] = {
    {  30, 18, 160, 68, "PREV",   "[L]" },
    { 210, 18, 160, 68, "PLAY",   "[X]" },
    { 390, 18, 160, 68, "NEXT",   "[R]" },
    { 570, 18, 160, 68, "SHUFFLE","[SQ]" },
    { 750, 18, 160, 68, "REPEAT", "[TRI]" }
};

const DeckButtonRect *input_get_button_rects(void) {
    return s_deck_buttons;
}

void input_init(void) {
#if defined(__psp2__) || defined(__VITA__)
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
#endif
    s_prev_buttons = 0;
    s_prev_touch = false;
    s_touch_held_btn = -1;
}

static int hit_test_deck_buttons(int x, int y) {
    for (int i = 0; i < DECK_BTN_COUNT; i++) {
        const DeckButtonRect *btn = &s_deck_buttons[i];
        if (x >= btn->x && x <= btn->x + btn->w &&
            y >= btn->y && y <= btn->y + btn->h) {
            return i;
        }
    }
    return -1;
}

void input_poll(InputState *state) {
    if (!state) return;
    memset(state, 0, sizeof(InputState));
    state->pressed_button = -1;

#if defined(__psp2__) || defined(__VITA__)
    /* 1. Physical Gamepad Input */
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);

    state->raw_buttons = pad.buttons;
    state->pressed_buttons = pad.buttons & ~s_prev_buttons;
    s_prev_buttons = pad.buttons;

    /* 2. Front Touchscreen Input */
    SceTouchData touch;
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

    if (touch.reportNum > 0) {
        state->touch_active = true;
        /* Vita touch sensor is 1920x1088; scale to 960x544 screen space */
        state->touch_x = touch.report[0].x / 2;
        state->touch_y = touch.report[0].y / 2;

        int hovered_btn = hit_test_deck_buttons(state->touch_x, state->touch_y);
        state->pressed_button = hovered_btn;
        s_touch_held_btn = hovered_btn;
        s_prev_touch = true;
    } else {
        if (s_prev_touch) {
            /* Finger was released! If released over a button, synthesize press */
            if (s_touch_held_btn >= 0) {
                switch (s_touch_held_btn) {
                    case BTN_INDEX_PREV:
                        state->pressed_buttons |= SCE_CTRL_LTRIGGER;
                        break;
                    case BTN_INDEX_PLAY_PAUSE:
                        state->pressed_buttons |= SCE_CTRL_CROSS;
                        break;
                    case BTN_INDEX_NEXT:
                        state->pressed_buttons |= SCE_CTRL_RTRIGGER;
                        break;
                    case BTN_INDEX_SHUFFLE:
                        state->pressed_buttons |= SCE_CTRL_SQUARE;
                        break;
                    case BTN_INDEX_REPEAT:
                        state->pressed_buttons |= SCE_CTRL_TRIANGLE;
                        break;
                }
            }
        }
        s_prev_touch = false;
        s_touch_held_btn = -1;
    }

    /* If a physical button is held down, also trigger visual sunken feedback */
    if (state->raw_buttons & SCE_CTRL_LTRIGGER) state->pressed_button = BTN_INDEX_PREV;
    else if (state->raw_buttons & SCE_CTRL_CROSS) state->pressed_button = BTN_INDEX_PLAY_PAUSE;
    else if (state->raw_buttons & SCE_CTRL_RTRIGGER) state->pressed_button = BTN_INDEX_NEXT;
    else if (state->raw_buttons & SCE_CTRL_SQUARE) state->pressed_button = BTN_INDEX_SHUFFLE;
    else if (state->raw_buttons & SCE_CTRL_TRIANGLE) state->pressed_button = BTN_INDEX_REPEAT;

#else
    /* Non-Vita mock */
    state->pressed_button = -1;
#endif
}
