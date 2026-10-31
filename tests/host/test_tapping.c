// Host unit tests for src/smk/tapping.c: LT / MT decisions, the queue behind an undecided key, and the
// replay. The test harness plays the part of matrix.c's process_key_state().

#include "tapping.h"
#include "keycodes.h"
#include <stdio.h>
#include <string.h>

// ---- the clock ----------------------------------------------------------------------------------
static uint16_t now;

uint16_t timer_read(void)
{
    return now;
}

uint16_t timer_elapsed(uint16_t since)
{
    return (uint16_t)(now - since);
}

// ---- a 1x6 "keyboard": the keycode each key has ---------------------------------------------------
#define A KC_A
static const uint16_t map[6] = {
    LT(1, KC_SPACE), // key 0
    KC_A,            // key 1
    KC_B,            // key 2
    MT(MOD_LSFT, KC_Z), // key 3
    LT(2, KC_ENTER), // key 4
    MT(MOD_RCTL | MOD_RALT, KC_X), // key 5
};

static int failures;
static int checks;

#define CHECK(cond)                                           \
    do {                                                      \
        checks++;                                             \
        if (!(cond)) {                                        \
            failures++;                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);    \
        }                                                     \
    } while (0)

// ---- what happened, in order ----------------------------------------------------------------------
typedef struct {
    char    what; // 'T' tap, 'M' mods, 'L' layer, 'k' key press, 'K' key release
    uint8_t value;
    bool    on;
} log_t;

static log_t log[64];
static int   log_count;

static void note(char what, uint8_t value, bool on)
{
    if (log_count < 64) {
        log[log_count].what  = what;
        log[log_count].value = value;
        log[log_count].on    = on;
        log_count++;
    }
}

static void process_key_state(uint8_t row, uint8_t col, bool pressed);

void tapping_hook_replay(uint8_t row, uint8_t col, bool pressed)
{
    process_key_state(row, col, pressed);
}

void tapping_hook_tap(uint8_t kc)
{
    note('T', kc, true);
}

void tapping_hook_mods(uint8_t mods, bool on)
{
    note('M', mods, on);
}

void tapping_hook_layer(uint8_t layer, bool on)
{
    note('L', layer, on);
}

// the part of matrix.c that decides what a key event is
static uint16_t down[6];

static void process_key_state(uint8_t row, uint8_t col, bool pressed)
{
    (void)row;
    if (tapping_intercept(row, col, pressed)) {
        return;
    }

    uint16_t kc;
    if (pressed) {
        kc        = map[col];
        down[col] = kc;
    } else {
        kc        = down[col];
        down[col] = KC_NO;
    }

    if (tapping_event(row, col, kc, pressed)) {
        return;
    }

    note(pressed ? 'k' : 'K', (uint8_t)kc, true);
}

static void press(uint8_t key)
{
    process_key_state(0, key, true);
}

static void release(uint8_t key)
{
    process_key_state(0, key, false);
}

static void wait(uint16_t ms)
{
    now += ms;
    tapping_task();
}

static void reset(void)
{
    tapping_init();
    log_count = 0;
    memset(down, 0, sizeof down);
}

static bool is(int i, char what, uint8_t value, bool on)
{
    return i < log_count && log[i].what == what && log[i].value == value && log[i].on == on;
}

// ---- tests -------------------------------------------------------------------------------------------
static void test_plain_keys_are_not_touched(void)
{
    reset();
    press(1);
    release(1);
    CHECK(log_count == 2 && is(0, 'k', KC_A, true) && is(1, 'K', KC_A, true));
}

static void test_tap_sends_the_key(void)
{
    reset();
    press(0);
    CHECK(log_count == 0); // undecided
    wait(100);
    release(0);
    CHECK(log_count == 1 && is(0, 'T', KC_SPACE, true));
}

static void test_hold_switches_the_layer(void)
{
    reset();
    press(0);
    wait(TAPPING_TERM - 1);
    CHECK(log_count == 0);
    wait(1);
    CHECK(log_count == 1 && is(0, 'L', 1, true));
    release(0);
    CHECK(log_count == 2 && is(1, 'L', 1, false));
}

static void test_mod_tap(void)
{
    reset();
    press(3);
    wait(TAPPING_TERM);
    CHECK(is(0, 'M', 0x02, true)); // left shift
    release(3);
    CHECK(is(1, 'M', 0x02, false));

    log_count = 0;
    press(3);
    release(3);
    CHECK(log_count == 1 && is(0, 'T', KC_Z, true));
}

static void test_right_hand_mods(void)
{
    reset();
    press(5);
    wait(TAPPING_TERM);
    CHECK(is(0, 'M', 0x50, true)); // right ctrl (0x10) + right alt (0x40)
    release(5);
    CHECK(is(1, 'M', 0x50, false));
}

static void test_key_typed_while_waiting_follows_the_decision(void)
{
    reset();
    press(0);
    wait(50);
    press(1); // typed during the wait: held back
    release(1);
    CHECK(log_count == 0);

    wait(TAPPING_TERM); // the wait ends as a hold: layer on, then the held-back key
    CHECK(is(0, 'L', 1, true) && is(1, 'k', KC_A, true) && is(2, 'K', KC_A, true));
    release(0);
    CHECK(is(3, 'L', 1, false));
}

static void test_tap_then_the_queued_keys(void)
{
    reset();
    press(0);
    press(1);
    release(0); // let go before the term: a tap, and the queued key comes after it
    CHECK(is(0, 'T', KC_SPACE, true) && is(1, 'k', KC_A, true));
    release(1);
    CHECK(is(2, 'K', KC_A, true));
}

static void test_release_of_a_key_pressed_before_the_wait(void)
{
    reset();
    press(1);
    press(0);
    release(1); // key 1 went down before the tap-hold key: its release must not get stuck behind the wait
    wait(TAPPING_TERM);
    // the release is replayed after the hold decision
    CHECK(is(0, 'k', KC_A, true));
    CHECK(is(1, 'L', 1, true));
    CHECK(is(2, 'K', KC_A, true));
}

static void test_second_tap_hold_key_waits_behind_the_first(void)
{
    reset();
    press(0);
    press(4); // another tap-hold key, during the first one's wait
    release(0); // first is a tap; then the second starts its own wait
    CHECK(is(0, 'T', KC_SPACE, true) && log_count == 1);
    wait(TAPPING_TERM);
    CHECK(is(1, 'L', 2, true));
    release(4);
    CHECK(is(2, 'L', 2, false));
}

static void test_queue_overflow_forces_the_hold(void)
{
    reset();
    press(0);
    for (int i = 0; i < 8; i++) { // 8 events fill the queue
        press(1);
        release(1);
        if (i == 3) {
            break;
        }
    }
    press(2); // the queue is full: the wait ends as a hold and this event goes through after it
    CHECK(is(0, 'L', 1, true));
}

int main(void)
{
    test_plain_keys_are_not_touched();
    test_tap_sends_the_key();
    test_hold_switches_the_layer();
    test_mod_tap();
    test_right_hand_mods();
    test_key_typed_while_waiting_follows_the_decision();
    test_tap_then_the_queued_keys();
    test_release_of_a_key_pressed_before_the_wait();
    test_second_tap_hold_key_waits_behind_the_first();
    test_queue_overflow_forces_the_hold();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
