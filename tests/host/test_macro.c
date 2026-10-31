// Host unit tests for src/smk/macro.c: the buffer format Vial writes, played back through hooks.

#include "macro.h"
#include "keycodes.h"
#include <stdio.h>
#include <string.h>

// ---- the buffer, as keymap_store.c would serve it -------------------------------------------------
static uint8_t buffer[KEYMAP_MACRO_BYTES];

void keymap_store_macro_read(uint16_t offset, uint8_t size, uint8_t *dst)
{
    for (uint8_t i = 0; i < size; i++) {
        dst[i] = (offset + i < KEYMAP_MACRO_BYTES) ? buffer[offset + i] : 0;
    }
}

// ---- what the macro did, in order -----------------------------------------------------------------
typedef struct {
    char     what; // 'p' press, 'r' release, 'w' wait
    uint16_t value;
} event_t;

static event_t events[128];
static int     event_count;

static void note(char what, uint16_t value)
{
    if (event_count < 128) {
        events[event_count].what  = what;
        events[event_count].value = value;
        event_count++;
    }
}

void macro_hook_press(uint16_t keycode)
{
    note('p', keycode);
}

void macro_hook_release(uint16_t keycode)
{
    note('r', keycode);
}

void macro_hook_wait_ms(uint16_t ms)
{
    note('w', ms);
}

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

static void load(const uint8_t *bytes, int n)
{
    memset(buffer, 0, sizeof buffer);
    memcpy(buffer, bytes, (size_t)n);
    event_count = 0;
}

static int is(int i, char what, uint16_t value)
{
    return i < event_count && events[i].what == what && events[i].value == value;
}

// a typed character is press+release of its key; shifted ones are wrapped in Left Shift
static int typed(int i, uint8_t hid, int shifted)
{
    if (shifted) {
        return is(i, 'p', KC_LSFT) && is(i + 1, 'p', hid) && is(i + 2, 'r', hid) && is(i + 3, 'r', KC_LSFT);
    }
    return is(i, 'p', hid) && is(i + 1, 'r', hid);
}

static void test_plain_text(void)
{
    const uint8_t m[] = { 'a', 'B', '1', '!', ' ', 0 };
    load(m, sizeof m);
    macro_play(0);
    CHECK(typed(0, KC_A, 0));
    CHECK(typed(2, KC_B, 1));
    CHECK(typed(6, KC_1, 0));
    CHECK(typed(8, KC_1, 1)); // '!' is shift + 1
    CHECK(typed(12, KC_SPACE, 0));
    CHECK(event_count == 14);
}

static void test_the_us_layout(void)
{
    // a few of the awkward ones: [HID usage, shifted]
    static const struct {
        char    c;
        uint8_t hid;
        int     shifted;
    } cases[] = {
        { '"', 0x34, 1 }, { '\'', 0x34, 0 }, { '@', 0x1F, 1 }, { '-', 0x2D, 0 }, { '_', 0x2D, 1 }, { '=', 0x2E, 0 },
        { '+', 0x2E, 1 }, { '[', 0x2F, 0 }, { '{', 0x2F, 1 }, { '\\', 0x31, 0 }, { '|', 0x31, 1 }, { ';', 0x33, 0 },
        { ':', 0x33, 1 }, { '`', 0x35, 0 }, { '~', 0x35, 1 }, { '/', 0x38, 0 }, { '?', 0x38, 1 }, { '0', 0x27, 0 },
        { ')', 0x27, 1 }, { 'z', 0x1D, 0 }, { 'Z', 0x1D, 1 }, { '^', 0x23, 1 }, { '<', 0x36, 1 }, { '>', 0x37, 1 },
    };

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const uint8_t m[] = { (uint8_t)cases[i].c, 0 };
        load(m, sizeof m);
        macro_play(0);
        CHECK(typed(0, cases[i].hid, cases[i].shifted));
    }
}

static void test_control_characters(void)
{
    const uint8_t m[] = { '\n', '\t', '\b', 0 };
    load(m, sizeof m);
    macro_play(0);
    CHECK(typed(0, KC_ENT, 0) && typed(2, KC_TAB, 0) && typed(4, KC_BSPC, 0));
}

static void test_picks_the_right_macro(void)
{
    const uint8_t m[] = { 'a', 0, 'b', 'c', 0, 0, 'd', 0 }; // macro 0 = "a", 1 = "bc", 2 = "", 3 = "d"
    load(m, sizeof m);
    macro_play(1);
    CHECK(event_count == 4 && typed(0, KC_B, 0) && typed(2, KC_C, 0));

    event_count = 0;
    macro_play(2);
    CHECK(event_count == 0);

    macro_play(3);
    CHECK(event_count == 2 && typed(0, KC_D, 0));

    event_count = 0;
    macro_play(KEYMAP_MACRO_COUNT); // there is no such macro
    CHECK(event_count == 0);
}

static void test_key_actions(void)
{
    // tap, down, up of a basic keycode, and the 16-bit forms
    const uint8_t m[] = { 1, 1, KC_X, 1, 2, KC_LCTL, 1, 3, KC_LCTL, 1, 5, 0x2A, 0x00 | 0x01, 1, 6, 0x01, 0xFF, 0 };
    load(m, 11);
    macro_play(0);
    CHECK(typed(0, KC_X, 0));
    CHECK(is(2, 'p', KC_LCTL) && is(3, 'r', KC_LCTL));

    // 0x0100 cannot be stored (a 0 byte ends the macro), so it is written 0xFF01: low byte 0x01, high byte 0xFF
    const uint8_t ext[] = { 1, 6, 0x01, 0xFF, 1, 7, 0x01, 0xFF, 0 };
    load(ext, sizeof ext);
    macro_play(0);
    CHECK(event_count == 2 && is(0, 'p', 0x0100) && is(1, 'r', 0x0100));
}

static void test_delay(void)
{
    // delay bytes are kept one higher: 301 ms = (d0-1) + (d1-1)*255 -> d0 = 47, d1 = 3 gives 46 + 510 = 556
    const uint8_t m[] = { 'a', 1, 4, 47, 3, 'b', 0 };
    load(m, sizeof m);
    macro_play(0);
    CHECK(typed(0, KC_A, 0) && is(2, 'w', 556) && typed(3, KC_B, 0));

    const uint8_t cut[] = { 'a', 1, 4, 5, 0 }; // a delay with a missing byte ends the macro
    load(cut, sizeof cut);
    macro_play(0);
    CHECK(event_count == 2);
}

static void test_damaged_buffers_play_nothing(void)
{
    const uint8_t m[] = { 'a', 'b', 'c', 0 };
    load(m, sizeof m);
    buffer[KEYMAP_MACRO_BYTES - 1] = 'x'; // the last byte must be 0: otherwise a write was cut short
    macro_play(0);
    CHECK(event_count == 0);
}

static void test_truncated_actions_stop_cleanly(void)
{
    const uint8_t m[] = { 'a', 1, 0 }; // a prefix and nothing after it
    load(m, sizeof m);
    macro_play(0);
    CHECK(event_count == 2 && typed(0, KC_A, 0));

    const uint8_t n[] = { 'a', 1, 1, 0 }; // a tap without its key
    load(n, sizeof n);
    macro_play(0);
    CHECK(event_count == 2);
}

int main(void)
{
    test_plain_text();
    test_the_us_layout();
    test_control_characters();
    test_picks_the_right_macro();
    test_key_actions();
    test_delay();
    test_damaged_buffers_play_nothing();
    test_truncated_actions_stop_cleanly();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
