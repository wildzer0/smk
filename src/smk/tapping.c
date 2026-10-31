#include "tapping.h"
#include "keycodes.h"
#include "timer.h"

#define QUEUE_SIZE 8u

typedef struct {
    uint8_t row;
    uint8_t col;
    bool    pressed;
} queued_event_t;

static bool           pending; // a tap-hold key is down and not decided yet
static uint8_t        pending_row;
static uint8_t        pending_col;
static uint16_t       pending_keycode;
static uint16_t       pending_since;

static queued_event_t queue[QUEUE_SIZE];
static uint8_t        queued;

static bool draining; // replaying the queue: events go straight through

void tapping_init(void)
{
    pending  = false;
    queued   = 0;
    draining = false;
}

bool tapping_is_tap_hold(uint16_t keycode)
{
    return IS_QK_LAYER_TAP(keycode) || IS_QK_MOD_TAP(keycode);
}

// QMK's 5-bit modifier mask: the low four bits pick the modifiers, bit 4 moves them to the right hand
static uint8_t hid_mods(uint8_t mask)
{
    return (mask & 0x10u) ? (uint8_t)((mask & 0x0Fu) << 4) : (uint8_t)(mask & 0x0Fu);
}

static void hold_action(uint16_t keycode, bool on)
{
    if (IS_QK_LAYER_TAP(keycode)) {
        tapping_hook_layer((uint8_t)QK_LAYER_TAP_GET_LAYER(keycode), on);
    } else {
        tapping_hook_mods(hid_mods((uint8_t)QK_MOD_TAP_GET_MODS(keycode)), on);
    }
}

// Replays what piled up behind the decision. An event may itself start a new wait, in which case the
// rest stays queued behind it.
static void drain(void)
{
    draining = true;

    uint8_t at = 0;
    while (at < queued && !pending) {
        const queued_event_t event = queue[at++];
        tapping_hook_replay(event.row, event.col, event.pressed);
    }

    // what was not replayed (a new wait began) moves to the front
    uint8_t left = 0;
    while (at < queued) {
        queue[left++] = queue[at++];
    }
    queued = left;

    draining = false;
}

static void decide_hold(void)
{
    pending = false;
    hold_action(pending_keycode, true);
    drain();
}

static void decide_tap(void)
{
    pending = false;
    tapping_hook_tap((uint8_t)(IS_QK_LAYER_TAP(pending_keycode) ? QK_LAYER_TAP_GET_TAP_KEYCODE(pending_keycode) : QK_MOD_TAP_GET_TAP_KEYCODE(pending_keycode)));
    drain();
}

bool tapping_intercept(uint8_t row, uint8_t col, bool pressed)
{
    if (!pending) {
        return false;
    }

    if (!pressed && row == pending_row && col == pending_col) {
        decide_tap(); // let go before the term: a tap
        return true;
    }

    if (queued < QUEUE_SIZE) {
        queue[queued].row     = row;
        queue[queued].col     = col;
        queue[queued].pressed = pressed;
        queued++;
        return true;
    }

    // no room to keep waiting: the wait becomes a hold and this event is handled after it
    decide_hold();
    return false;
}

bool tapping_event(uint8_t row, uint8_t col, uint16_t keycode, bool pressed)
{
    if (!tapping_is_tap_hold(keycode)) {
        return false;
    }

    if (pressed) {
        pending         = true;
        pending_row     = row;
        pending_col     = col;
        pending_keycode = keycode;
        pending_since   = timer_read();
    } else {
        hold_action(keycode, false); // the release of a key that became a hold
    }

    return true;
}

void tapping_task(void)
{
    if (pending && !draining && timer_elapsed(pending_since) >= TAPPING_TERM) {
        decide_hold();
    }
}
