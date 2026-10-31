#include "kbdef.h"
#include "layout.h"
#include "user_layout.h"
#include "report.h"
#include "host.h"
#include <stdint.h>

// clang-format off

// Probe layout: finds which key sits on which (column, row) of the matrix. Every cell is a
// custom keycode, and pressing it types that cell's coordinates ("c03r2 ") into whatever has
// the keyboard focus. Open a text editor, press the keys in a known order, and the typed text
// is the map from physical key to matrix cell.
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
