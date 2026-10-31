#include "tapping.h"
#include "keycodes.h"
#include "timer.h"
#include "keymap_store.h"

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

#ifdef KEYMAP_TAP_DANCE_COUNT
#    define TD_ON_TAP        0
#    define TD_ON_HOLD       1
#    define TD_ON_DOUBLE_TAP 2
#    define TD_TERM          4

static bool     second_wait;                              // the first tap is over, waiting for a second press
static uint16_t td_held[KEYMAP_TAP_DANCE_COUNT];          // the keycode a tap dance key holds down, 0 = none

static uint8_t pending_td(void)
{
    return (uint8_t)(pending_keycode - QK_TAP_DANCE);
}

static uint16_t td_term(void)
{
    const uint16_t t = keymap_store_tap_dance_get(pending_td(), TD_TERM);
    return t ? t : TAPPING_TERM;
}

// An action that is itself a tap dance would recurse: it does nothing
static uint16_t td_action(uint8_t field)
{
    const uint16_t kc = keymap_store_tap_dance_get(pending_td(), field);
    return IS_QK_TAP_DANCE(kc) ? 0 : kc;
}
#endif

static bool draining; // replaying the queue: events go straight through

void tapping_init(void)
{
    pending  = false;
    queued   = 0;
    draining = false;
#ifdef KEYMAP_TAP_DANCE_COUNT
    second_wait = false;
    for (uint8_t i = 0; i < KEYMAP_TAP_DANCE_COUNT; i++) {
        td_held[i] = 0;
    }
#endif
}

bool tapping_is_tap_hold(uint16_t keycode)
{
#ifdef KEYMAP_TAP_DANCE_COUNT
    if (IS_QK_TAP_DANCE(keycode)) {
        return (keycode - QK_TAP_DANCE) < KEYMAP_TAP_DANCE_COUNT;
    }
#endif
    return IS_QK_LAYER_TAP(keycode) || IS_QK_MOD_TAP(keycode);
}

// QMK's 5-bit modifier mask: the low four bits pick the modifiers, bit 4 moves them to the right hand
static uint8_t hid_mods(uint8_t mask)
{
    return (mask & 0x10u) ? (uint8_t)((mask & 0x0Fu) << 4) : (uint8_t)(mask & 0x0Fu);
}

static void hold_action(uint16_t keycode, bool on)
{
#ifdef KEYMAP_TAP_DANCE_COUNT
    if (IS_QK_TAP_DANCE(keycode)) {
        const uint8_t index = (uint8_t)(keycode - QK_TAP_DANCE);
        if (on) {
            uint16_t kc = td_action(TD_ON_HOLD);
            if (!kc) {
                kc = td_action(TD_ON_TAP);
            }
            td_held[index] = kc;
            if (kc) {
                tapping_hook_keycode(kc, true);
            }
        } else if (td_held[index]) {
            tapping_hook_keycode(td_held[index], false);
            td_held[index] = 0;
        }
        return;
    }
#endif
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
#ifdef KEYMAP_TAP_DANCE_COUNT
    if (IS_QK_TAP_DANCE(pending_keycode)) {
        second_wait = false;
        const uint16_t kc = td_action(TD_ON_TAP);
        if (kc) {
            tapping_hook_keycode(kc, true);
            tapping_hook_keycode(kc, false);
        }
        drain();
        return;
    }
#endif
    tapping_hook_tap((uint8_t)(IS_QK_LAYER_TAP(pending_keycode) ? QK_LAYER_TAP_GET_TAP_KEYCODE(pending_keycode) : QK_MOD_TAP_GET_TAP_KEYCODE(pending_keycode)));
    drain();
}

bool tapping_intercept(uint8_t row, uint8_t col, bool pressed)
{
    if (!pending) {
        return false;
    }

#ifdef KEYMAP_TAP_DANCE_COUNT
    if (second_wait && pressed && row == pending_row && col == pending_col) {
        // the second tap: "on double tap" goes down now and up when this key does
        second_wait = false;
        pending     = false;
        td_held[pending_td()] = td_action(TD_ON_DOUBLE_TAP);
        if (td_held[pending_td()]) {
            tapping_hook_keycode(td_held[pending_td()], true);
        }
        drain();
        return true;
    }
#endif

    if (!pressed && row == pending_row && col == pending_col) {
#ifdef KEYMAP_TAP_DANCE_COUNT
        if (IS_QK_TAP_DANCE(pending_keycode) && !second_wait && td_action(TD_ON_DOUBLE_TAP)) {
            second_wait   = true; // maybe a double tap: wait one more term
            pending_since = timer_read();
            return true;
        }
#endif
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

    // no room to keep waiting: the wait becomes a hold (a tap, if the key is already up) and this event is handled after it
#ifdef KEYMAP_TAP_DANCE_COUNT
    if (second_wait) {
        decide_tap();
        return false;
    }
#endif
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
#ifdef KEYMAP_TAP_DANCE_COUNT
        second_wait = false;
#endif
    } else {
        hold_action(keycode, false); // the release of a key that became a hold
    }

    return true;
}

void tapping_task(void)
{
    if (!pending || draining) {
        return;
    }

    uint16_t term = TAPPING_TERM;
#ifdef KEYMAP_TAP_DANCE_COUNT
    if (IS_QK_TAP_DANCE(pending_keycode)) {
        term = td_term();
    }
#endif

    if (timer_elapsed(pending_since) >= term) {
#ifdef KEYMAP_TAP_DANCE_COUNT
        if (second_wait) {
            decide_tap(); // no second tap came
            return;
        }
#endif
        decide_hold();
    }
}
