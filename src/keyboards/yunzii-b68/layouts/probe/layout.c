#include "kbdef.h"
#include "layout.h"
#include "user_layout.h"
#include "report.h"
#include "host.h"
#include "timer.h"
#include "user_matrix.h"
#include "delay.h"
#include <stdint.h>

// clang-format off

// Probe layout: finds which key sits on which (column, row) of the matrix. Every cell is a
// custom keycode, and pressing it types that cell's coordinates ("c03r2 ") into whatever has
// the keyboard focus. Open a text editor, press the keys in a known order, and the typed text
// is the map from physical key to matrix cell.
//
// It also watches the input pins and types a line whenever one changes: "p70=0 " means P7.0 went low.
// "e07=0 " means P7.0 is pulled low while column 7 is driven (an encoder wired as a matrix cell).
// Leave the keyboard alone for the first 4 s after plugging it in (it learns which pins are noisy),
// then turn the encoder slowly: the lines that appear are its contacts. See kb_update() below.
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

// The monitor watches every pin the stock firmware configures as an input (PxCR bit clear), except
// the USB data pins (P0.0 and P0.1), which move with every packet. For the first few seconds after
// power-up it only learns which of them move by themselves (the radio's lines, say); keep your hands
// off the keyboard in that time. After that, a change on any other pin is typed as "pPB=L " (port P,
// bit B, new level L). The rows are read from the main loop, where no column is being driven, so
// pressing keys does not move them.
#define LEARN_MS 4000u
#define MAX_EVENTS 60u

static uint8_t read_port(uint8_t port)
{
    switch (port) {
        case 0:
            return P0;
        case 1:
            return P1;
        case 2:
            return P2;
        case 3:
            return P3;
        case 4:
            return P4;
        case 5:
            return P5;
        case 6:
            return P6;
        default:
            return P7;
    }
}

static uint8_t read_direction(uint8_t port)
{
    switch (port) {
        case 0:
            return P0CR;
        case 1:
            return P1CR;
        case 2:
            return P2CR;
        case 3:
            return P3CR;
        case 4:
            return P4CR;
        case 5:
            return P5CR;
        case 6:
            return P6CR;
        default:
            return P7CR;
    }
}

// P7.0 is a sixth sense line the stock firmware reads next to the five matrix rows. If the encoder
// sits on it as extra matrix cells (one per direction), they only show while their column is driven.
// So drive each column in turn and keep the ones that pull P7.0 low. Interrupts are off meanwhile so
// the matrix scan cannot drive a column at the same time.
static uint16_t read_row5(void)
{
    uint16_t cells = 0;

    __critical
    {
        user_matrix_cols_deselect_all();
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            user_matrix_col_select(col);
            delay_us(10);
            if (!(P7 & 0x01u)) {
                cells |= (uint16_t)(1u << col);
            }
            user_matrix_col_deselect(col);
        }
    }

    return cells;
}

void kb_update(void)
{
    static uint16_t last_cells;
    static uint16_t noisy_cells;
    static uint8_t  last[8];
    static uint8_t  noisy[8];
    static uint16_t started;
    static uint8_t  events;
    static bool     primed;

    const uint16_t cells = read_row5();

    if (!primed) {
        last_cells = cells;
        for (uint8_t port = 0; port < 8; port++) {
            last[port] = read_port(port);
        }
        started = timer_read();
        primed  = true;
        return;
    }

    const bool learning = timer_elapsed(started) < LEARN_MS;

    for (uint8_t port = 0; port < 8; port++) {
        uint8_t watch = (uint8_t)~read_direction(port); // inputs only
        if (port == 0) {
            watch &= (uint8_t)~0x03u; // USB D+ / D-
        }

        const uint8_t value   = read_port(port);
        const uint8_t changed = (uint8_t)((value ^ last[port]) & watch);

        last[port] = value;

        if (learning) {
            noisy[port] |= changed;
            continue;
        }

        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((changed & (uint8_t)~noisy[port]) & (uint8_t)(1u << bit)) {
                if (events < MAX_EVENTS) {
                    events++;
                    tap(KC_P);
                    tap_digit(port);
                    tap_digit(bit);
                    tap(KC_EQUAL);
                    tap_digit((value >> bit) & 1u);
                    tap(KC_SPACE);
                }
            }
        }
    }

    const uint16_t cell_changes = cells ^ last_cells;
    last_cells                  = cells;

    if (learning) {
        noisy_cells |= cell_changes;
    } else {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if ((cell_changes & (uint16_t)~noisy_cells) & (uint16_t)(1u << col)) {
                if (events < MAX_EVENTS) {
                    events++;
                    tap(KC_E);
                    tap_digit(col / 10);
                    tap_digit(col % 10);
                    tap(KC_EQUAL);
                    tap_digit(((cells >> col) & 1u) ? 0 : 1); // 0 = P7.0 pulled low
                    tap(KC_SPACE);
                }
            }
        }
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
