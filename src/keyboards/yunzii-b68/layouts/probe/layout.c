#include "kbdef.h"
#include "layout.h"
#include "user_layout.h"
#include "report.h"
#include "host.h"
#include "user_matrix.h"
#include <stdint.h>

// clang-format off

// Probe layout: finds which key sits on which (column, row) of the matrix. Every cell is a
// custom keycode, and pressing it types that cell's coordinates ("c03r2 ") into whatever has
// the keyboard focus. Open a text editor, press the keys in a known order, and the typed text
// is the map from physical key to matrix cell.
//
// It also watches the inputs that are not part of the matrix and types a line whenever one of them
// changes: "p70=0 " means P7.0 went low. Turn the encoder slowly and the lines that appear are its
// contacts. "i05 " is the set of matrix rows (bit 0 = row 0) held low while no column is driven, i.e.
// a contact wired straight to ground.
//
// Build: meson compile -C <build> yunzii-b68_probe_smk.hex

#define PROBE_BASE QK_USER
#define P(col, row) (PROBE_BASE + (col) * MATRIX_ROWS + (row))

#define _BL 0

const uint16_t keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BL] = {
        { P(0, 0), P(1, 0), P(2, 0), P(3, 0), P(4, 0), P(5, 0), P(6, 0), P(7, 0), P(8, 0), P(9, 0), P(10, 0), P(11, 0), P(12, 0), P(13, 0), P(14, 0), P(15, 0) },
        { P(0, 1), P(1, 1), P(2, 1), P(3, 1), P(4, 1), P(5, 1), P(6, 1), P(7, 1), P(8, 1), P(9, 1), P(10, 1), P(11, 1), P(12, 1), P(13, 1), P(14, 1), P(15, 1) },
        { P(0, 2), P(1, 2), P(2, 2), P(3, 2), P(4, 2), P(5, 2), P(6, 2), P(7, 2), P(8, 2), P(9, 2), P(10, 2), P(11, 2), P(12, 2), P(13, 2), P(14, 2), P(15, 2) },
        { P(0, 3), P(1, 3), P(2, 3), P(3, 3), P(4, 3), P(5, 3), P(6, 3), P(7, 3), P(8, 3), P(9, 3), P(10, 3), P(11, 3), P(12, 3), P(13, 3), P(14, 3), P(15, 3) },
        { P(0, 4), P(1, 4), P(2, 4), P(3, 4), P(4, 4), P(5, 4), P(6, 4), P(7, 4), P(8, 4), P(9, 4), P(10, 4), P(11, 4), P(12, 4), P(13, 4), P(14, 4), P(15, 4) }
    }
};

const uint8_t keymap_layers = sizeof(keymaps) / sizeof(keymaps[0]);

// clang-format on

static void tap(uint8_t key)
{
    add_key(key);
    send_keyboard_report();
    del_key(key);
    send_keyboard_report();
}

// 0..9 as the digit keys (KC_1..KC_9 are consecutive, KC_0 follows them)
static void tap_digit(uint8_t digit)
{
    tap(digit == 0 ? KC_0 : (uint8_t)(KC_1 + digit - 1));
}

// Inputs worth watching: P7.0 (read by the stock firmware next to the rows, function unknown) and the
// other pulled-up inputs it leaves alone. P0.5-P0.7 are left out: they are the radio's lines on
// the Air60 and would flood the output.
typedef struct {
    uint8_t port; // 0, 4 or 7
    uint8_t bit;
} watched_pin_t;

static const __code watched_pin_t watched[] = { { 7, 0 }, { 7, 7 }, { 4, 6 }, { 0, 3 }, { 0, 4 } };

#define WATCHED_COUNT (sizeof(watched) / sizeof(watched[0]))

static uint8_t pin_level(uint8_t port, uint8_t bit)
{
    uint8_t value = 0;

    switch (port) {
        case 0:
            value = P0;
            break;
        case 4:
            value = P4;
            break;
        default:
            value = P7;
            break;
    }

    return (value >> bit) & 1u;
}

void kb_update(void)
{
    static uint8_t levels[WATCHED_COUNT];
    static uint8_t idle_rows;
    static bool    primed;

    uint8_t now[WATCHED_COUNT];
    uint8_t rows;

    // no ISR may move a column while the pins are read
    __critical
    {
        for (uint8_t i = 0; i < WATCHED_COUNT; i++) {
            now[i] = pin_level(watched[i].port, watched[i].bit);
        }
        rows = (uint8_t)(~user_matrix_read_rows() & 0x1Fu);
    }

    if (!primed) {
        for (uint8_t i = 0; i < WATCHED_COUNT; i++) {
            levels[i] = now[i];
        }
        idle_rows = rows;
        primed    = true;
        return;
    }

    for (uint8_t i = 0; i < WATCHED_COUNT; i++) {
        if (now[i] != levels[i]) {
            levels[i] = now[i];
            tap(KC_P);
            tap_digit(watched[i].port);
            tap_digit(watched[i].bit);
            tap(KC_EQUAL);
            tap_digit(now[i]);
            tap(KC_SPACE);
        }
    }

    if (rows != idle_rows) {
        idle_rows = rows;
        tap(KC_I);
        tap_digit(rows / 10);
        tap_digit(rows % 10);
        tap(KC_SPACE);
    }
}

bool layout_process_record(uint16_t keycode, bool key_pressed)
{
    if (keycode < PROBE_BASE || keycode >= PROBE_BASE + MATRIX_COLS * MATRIX_ROWS) {
        return true;
    }

    if (key_pressed) {
        const uint8_t cell = (uint8_t)(keycode - PROBE_BASE);
        const uint8_t col  = cell / MATRIX_ROWS;
        const uint8_t row  = cell % MATRIX_ROWS;

        tap(KC_C);
        tap_digit(col / 10);
        tap_digit(col % 10);
        tap(KC_R);
        tap_digit(row);
        tap(KC_SPACE);
    }

    return false; // the keycode is not a real key: nothing for send_keycode() to do
}
