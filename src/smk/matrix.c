#include "matrix.h"
#include "report.h"
#include "debug.h"
#include "layout.h"
#include "user_layout.h"
#include "kb.h"
#include "user_matrix.h"
#include "kbdef.h"
#include "host.h"
#include "delay.h"
#include "indicators.h"
#include "sleep.h"
#include "layer.h"
#include "tapping.h"
#include "macro.h"
#include "watchdog.h"
#include <stdlib.h>
#include <stdbool.h>

typedef uint8_t matrix_col_t;

matrix_col_t matrix[MATRIX_COLS];
matrix_col_t matrix_previous[MATRIX_COLS];

volatile bool matrix_updated;

void matrix_init()
{
    layer_init();
    tapping_init();
    matrix_updated = false;

    for (int i = 0; i < MATRIX_COLS; i++) {
        matrix[i]          = 0;
        matrix_previous[i] = 0;
    }
}

void set_default_layer(uint8_t layer)
{
    layer_set_default(layer);
}

void layer_hook_mods_add(uint8_t mods)
{
    add_mods(mods);
    send_keyboard_report();
}

void layer_hook_mods_del(uint8_t mods)
{
    del_mods(mods);
    send_keyboard_report();
}

static void send_keycode(uint16_t qcode, bool pressed)
{
    if (IS_MODIFIER_KEYCODE(qcode)) {
        if (pressed) {
            add_mods(MOD_BIT((uint8_t)(qcode & 0xFF)));
        } else {
            del_mods(MOD_BIT((uint8_t)(qcode & 0xFF)));
        }
        send_keyboard_report();
        return;
    }

    if (IS_BASIC_KEYCODE(qcode)) {
        if (pressed) {
            add_key((uint8_t)(qcode & 0xFF));
        } else {
            del_key((uint8_t)(qcode & 0xFF));
        }
        send_keyboard_report();
        return;
    }

    if (IS_SYSTEM_KEYCODE(qcode)) {
        host_system_send(pressed ? keycode_to_system(qcode) : 0);
        return;
    }

    if (IS_CONSUMER_KEYCODE(qcode)) {
        host_consumer_send(pressed ? keycode_to_consumer(qcode) : 0);
        return;
    }
}

void matrix_tap_keycode(uint16_t keycode)
{
    send_keycode(keycode, true);
    send_keycode(keycode, false);
}

static void process_key_state(uint8_t row, uint8_t col, bool pressed);

#ifdef KEYMAP_MACRO_BYTES
void macro_hook_press(uint16_t keycode)
{
    send_keycode(keycode, true);
}

void macro_hook_release(uint16_t keycode)
{
    send_keycode(keycode, false);
}

void macro_hook_wait_ms(uint16_t ms)
{
    while (ms--) {
        watchdog_kick();
        delay_ms(1);
    }
}
#endif

// ---- what tapping.c needs from the rest of the firmware ----
void tapping_hook_replay(uint8_t row, uint8_t col, bool pressed)
{
    process_key_state(row, col, pressed);
}

void tapping_hook_tap(uint8_t basic_keycode)
{
    layer_key_pressed(); // a tapped key is a key press as far as one-shots go
    send_keycode(basic_keycode, true);
    send_keycode(basic_keycode, false);
    layer_key_released();
}

void tapping_hook_mods(uint8_t mods, bool on)
{
    if (on) {
        layer_hook_mods_add(mods);
    } else {
        layer_hook_mods_del(mods);
    }
}

void tapping_hook_layer(uint8_t layer, bool on)
{
    layer_hold(layer, on);
}

static void process_key_state(uint8_t row, uint8_t col, bool pressed)
{
    if (tapping_intercept(row, col, pressed)) {
        return; // a tap-or-hold key is undecided: this waits behind it (or decides it)
    }

    const uint16_t qcode = pressed ? layer_key_down(row, col) : layer_key_up(row, col);

    if (tapping_event(row, col, qcode, pressed)) {
        return; // a tap-or-hold key starts waiting, or lets go after becoming a hold
    }

    if (layer_process(qcode, pressed)) {
        return;
    }

#ifdef KEYMAP_MACRO_BYTES
    if (IS_QK_MACRO(qcode)) {
        if (pressed) {
            layer_key_pressed();
            macro_play((uint8_t)(qcode - QK_MACRO));
        } else {
            layer_key_released();
        }
        return;
    }
#endif

    if (pressed) {
        layer_key_pressed();
    }

    if (kb_process_record(qcode, pressed) && layout_process_record(qcode, pressed)) {
        send_keycode(qcode, pressed);
    }

    if (!pressed) {
        layer_key_released();
    }
}

void matrix_tap_task(void)
{
    tapping_task();
}

void matrix_scan_full(void)
{
    indicators_pwm_disable();

    user_matrix_sinks_off();

    user_matrix_scan_pre();
    user_matrix_cols_deselect_all();

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        user_matrix_col_select(col);

        delay_us(10); // let the row lines settle before sampling
        const uint8_t sample1 = user_matrix_read_rows();
        delay_us(10);
        const uint8_t sample2 = user_matrix_read_rows();

        if (sample1 == sample2) {
            matrix[col] = ~sample1;
        }

        user_matrix_col_deselect(col);
    }

    user_matrix_scan_post();

    indicators_pwm_enable();

    matrix_updated = true;
}

// True while any key is down, as of the last pass of matrix_task().
bool matrix_any_pressed(void)
{
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        if (matrix_previous[col]) {
            return true;
        }
    }

    return false;
}

bool matrix_is_on(uint8_t row, uint8_t col)
{
    return row < MATRIX_ROWS && col < MATRIX_COLS && (matrix_previous[col] & (matrix_col_t)(1u << row));
}

uint8_t matrix_task()
{
    if (!matrix_updated) {
        return false;
    }

    // Snapshot the scan-written matrix[], then diff it against
    // matrix_previous[]. No lock needed: each column byte reads atomically, so a
    // concurrent scan lands cleanly on one side of the read - at worst a
    // transition is split across two main-loop iterations, never lost.
    matrix_col_t snapshot[MATRIX_COLS];
    matrix_updated = false;
    for (uint8_t i = 0; i < MATRIX_COLS; i++) {
        snapshot[i] = matrix[i];
    }

    bool matrix_changed = false;

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        const matrix_col_t current_col = snapshot[col];
        const matrix_col_t col_changes = current_col ^ matrix_previous[col];
        if (!col_changes) {
            continue;
        }
        matrix_changed = true;
        sleep_note_activity(); // a key changed state; reset the inactivity timer

        matrix_col_t row_mask = 1;
        for (uint8_t row = 0; row < MATRIX_ROWS; row++, row_mask <<= 1) {
            if (col_changes & row_mask) {
                const bool key_pressed = current_col & row_mask;
                process_key_state(row, col, key_pressed);
            }
        }

        matrix_previous[col] = current_col;
    }

    return matrix_changed;
}
