#include "kbdef.h"
#include "layout.h"
#include "user_layout.h"
#include "report.h"
#include "host.h"
#include "timer.h"
#include "delay.h"
#include "tick.h"
#include "watchdog.h"
#include "user_matrix.h"
#include "pwm.h"
#include <stdint.h>

// clang-format off

// LED probe layout: the default keymap, plus a test that lights ONE LED channel at a time and types a label for each
// step, to find which pins drive the RGB backlight. See docs/led-probe.md. Nothing is stored.
//
// The matrix cells below were mapped on the board with the probe layout (docs/keyboards/yunzii-b68.md):
// 15 + 15 + 14 + 14 + 9 cells over 16 columns and 5 rows: 66 keys and the push of the rotary encoder
// (c15r0). The macro lists them left to right, top to bottom; the names are K<column>_<row>.
//
// The three keys under the encoder (c15r1..r3) are assumed to be Del, PgUp and PgDn. The encoder
// itself (rotation) is not read yet: it is not in the matrix.

#define LAYOUT_B68( \
                   K00_0, K01_0, K02_0, K03_0, K04_0, K05_0, K06_0, K07_0, K08_0, K09_0, K10_0, K11_0, K12_0, K13_0, K15_0, \
                   K00_1, K01_1, K02_1, K03_1, K04_1, K05_1, K06_1, K07_1, K08_1, K09_1, K10_1, K11_1, K12_1, K13_1, K15_1, \
                   K00_2, K01_2, K02_2, K03_2, K04_2, K05_2, K06_2, K07_2, K08_2, K09_2, K10_2, K11_2, K13_2, K15_2, \
                   K00_3, K01_3, K02_3, K03_3, K04_3, K05_3, K06_3, K07_3, K08_3, K09_3, K10_3, K13_3, K14_3, K15_3, \
                   K00_4, K01_4, K02_4, K05_4, K08_4, K09_4, K13_4, K14_4, K15_4 \
                 ) { \
    { K00_0, K01_0, K02_0, K03_0, K04_0, K05_0, K06_0, K07_0, K08_0, K09_0, K10_0, K11_0, K12_0, K13_0, KC_NO, K15_0 }, \
    { K00_1, K01_1, K02_1, K03_1, K04_1, K05_1, K06_1, K07_1, K08_1, K09_1, K10_1, K11_1, K12_1, K13_1, KC_NO, K15_1 }, \
    { K00_2, K01_2, K02_2, K03_2, K04_2, K05_2, K06_2, K07_2, K08_2, K09_2, K10_2, K11_2, KC_NO, K13_2, KC_NO, K15_2 }, \
    { K00_3, K01_3, K02_3, K03_3, K04_3, K05_3, K06_3, K07_3, K08_3, K09_3, K10_3, KC_NO, KC_NO, K13_3, K14_3, K15_3 }, \
    { K00_4, K01_4, K02_4, KC_NO, KC_NO, K05_4, KC_NO, KC_NO, K08_4, K09_4, KC_NO, KC_NO, KC_NO, K13_4, K14_4, K15_4 } \
}

#define _BL 0
#define _FL 1

const uint16_t keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /* Keymap _BL: Base layer
     * ,------------------------------------------------------------.
     * |Esc|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|  Backsp| Knob
     * |------------------------------------------------------------|
     * |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|     \| Del
     * |------------------------------------------------------------|
     * |Caps   |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '| Return | PgUp
     * |------------------------------------------------------------|
     * |Shift   |  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|  Shift| Up| PgDn
     * |------------------------------------------------------------|
     * |Ctrl|Win |Alt |      Space        |Alt| Fn|   | Lt| Dn| Rt
     * `------------------------------------------------------------'
     */
    [_BL] = LAYOUT_B68(
        KC_ESC, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_MINS, KC_EQL, KC_BSPC, KC_MUTE,
        KC_TAB, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_LBRC, KC_RBRC, KC_BSLS, KC_DEL,
        KC_CAPS, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCLN, KC_QUOT, KC_ENT, KC_PGUP,
        KC_LSFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT, KC_UP, KC_PGDN,
        KC_LCTL, KC_LGUI, KC_LALT, KC_SPC, KC_RALT, MO(1), KC_LEFT, KC_DOWN, KC_RGHT
    ),
    /* Keymap _FL: Fn layer: F1-F12 on the number row, media on the arrows */
    [_FL] = LAYOUT_B68(
        KC_GRV, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_VOLU, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_MPRV, KC_VOLD, KC_MNXT
    )
};

// Encoder: { counter-clockwise, clockwise } per layer. Volume on the base layer; the Fn layer leaves it as it is.
const uint16_t encoder_map[][ENCODER_COUNT][2] = {
    [_BL] = { { KC_VOLD, KC_VOLU } },
    [_FL] = { { KC_TRNS, KC_TRNS } },
};

const uint8_t keymap_layers = sizeof(keymaps) / sizeof(keymaps[0]);

// ---- the LED test -----------------------------------------------------------------------------------
//
// Hypothesis from the stock firmware: the 18 pins P1.0-5, P2.0-5, P3.0-5 are the LED "columns" (sources) and the 16 key-matrix strobe
// lines are the LED sinks. One LED lights when one column pin is high and one strobe is low. The test drives that for STEP_MS at about 25 %
// duty (software PWM, so no LED sees more than its normal multiplexed load), one combination per step, and types a label first.
//
//   A00..A15  column 0 (P1.0) high, strobe 0..15 low:  which key lights as the sink changes (its position tells the strobe's key column)
//   B00..B17  strobe 0 low, column 0..17 high:          which key and colour as the column changes
//   C00..C17  the same with strobe 8 low
//
// Column k is P1.(k) for k < 6, P2.(k-6) for 6 <= k < 12, P3.(k-12) for k >= 12.

// LED_STAGE 1 (default): A, B, C below. LED_STAGE 2: the status LED and the PWM waveform (D, E, F), see run_led_test().
#ifndef LED_STAGE
#    define LED_STAGE 1
#endif

#define START_MS 5000u // hands off the keyboard until it starts
#define STEP_MS  1500u
#define STEP_LOOPS 3750u // STEP_MS / 0.4 ms

static void tap_key(uint8_t key)
{
    add_key(key);
    send_keyboard_report();
    del_key(key);
    send_keyboard_report();
}

static void tap_digit(uint8_t digit)
{
    tap_key(digit == 0 ? KC_0 : (uint8_t)(KC_1 + digit - 1));
}

static void type_label(char phase, uint8_t number)
{
    tap_key((uint8_t)(KC_A + (phase - 'A')));
    tap_digit(number / 10);
    tap_digit(number % 10);
    tap_key(KC_SPACE);
}

static void column_pin(uint8_t k, bool high)
{
    const uint8_t mask = (uint8_t)(1u << (k % 6));

    switch (k / 6) {
        case 0:
            if (high) {
                P1 |= mask;
            } else {
                P1 &= (uint8_t)~mask;
            }
            break;
        case 1:
            if (high) {
                P2 |= mask;
            } else {
                P2 &= (uint8_t)~mask;
            }
            break;
        default:
            if (high) {
                P3 |= mask;
            } else {
                P3 &= (uint8_t)~mask;
            }
            break;
    }
}

// One LED channel for STEP_MS: strobe `sink` low, column pin `k` pulsed (100 us on, 300 us off).
static void light(uint8_t k, uint8_t sink)
{
    user_matrix_col_select(sink);

    for (uint16_t loop = 0; loop < STEP_LOOPS; loop++) {
        if ((loop & 3u) == 0) {
            watchdog_kick();
        }
        column_pin(k, true);
        delay_us(100);
        column_pin(k, false);
        delay_us(300);
    }

    user_matrix_col_deselect(sink);
}

// Lights `pin` ("P4.1" and so on, by port and bit) high for STEP_MS at about 25 % duty, columns left low (idle).
static void extra_pin(uint8_t port, uint8_t bit, bool high)
{
    const uint8_t mask = (uint8_t)(1u << bit);

    if (port == 4) {
        if (high) {
            P4 |= mask;
        } else {
            P4 &= (uint8_t)~mask;
        }
    } else {
        if (high) {
            P7 |= mask;
        } else {
            P7 &= (uint8_t)~mask;
        }
    }
}

static void pulse_extra_source(uint8_t port, uint8_t bit)
{
    for (uint16_t loop = 0; loop < STEP_LOOPS; loop++) {
        if ((loop & 3u) == 0) {
            watchdog_kick();
        }
        extra_pin(port, bit, true);
        delay_us(100);
        extra_pin(port, bit, false);
        delay_us(300);
    }
}

// The same pulsing on a column pin, the strobes left alone (the extra pins stay low, as at rest).
static void pulse_column(uint8_t k)
{
    for (uint16_t loop = 0; loop < STEP_LOOPS; loop++) {
        if ((loop & 3u) == 0) {
            watchdog_kick();
        }
        column_pin(k, true);
        delay_us(100);
        column_pin(k, false);
        delay_us(300);
    }
}

static void hold_ms(uint16_t ms)
{
    while (ms--) {
        watchdog_kick();
        delay_ms(1);
    }
}

// Column 3 (P1.3, the red of the top row) on the hardware PWM, the way the stock firmware sets it up (period 1200 counts), with
// the top-left key's strobe held low. DUTY1 is where the output goes one way and DUTY2 where it comes back; the width between
// them is the brightness. Tried both ways round, so the one that gives a proportional light shows which polarity it is.
static void pwm_step(uint16_t duty1, uint16_t duty2)
{
    PWM2PERDH = 0x04;
    PWM2PERDL = 0xB0;
    SET_PWM_DUTY_1(PWM23, duty1);
    SET_PWM_DUTY_2(PWM23, duty2);
    PWM23CON = 0x08; // the values the stock firmware writes: the first channel of a bank starts the bank
    PWM20CON = 0x89;

    user_matrix_col_select(0);
    hold_ms(STEP_MS);
    user_matrix_col_deselect(0);

    PWM20CON = 0;
    PWM23CON = 0;
}

static void run_led_test(void)
{
    tick_pause(); // the key scan would drive the same lines

#if LED_STAGE == 1
    for (uint8_t sink = 0; sink < 16; sink++) {
        type_label('A', sink);
        light(0, sink);
    }
    for (uint8_t k = 0; k < 18; k++) {
        type_label('B', k);
        light(k, 0);
    }
    for (uint8_t k = 0; k < 18; k++) {
        type_label('C', k);
        light(k, 8);
    }
#else
    static const __code uint8_t extras[4][2] = { { 4, 1 }, { 7, 4 }, { 7, 5 }, { 7, 6 } }; // port, bit

    // D00-D03: each extra output as a SOURCE (high), the LED columns at rest (low)
    for (uint8_t i = 0; i < 4; i++) {
        type_label('D', i);
        pulse_extra_source(extras[i][0], extras[i][1]);
    }
    // D04-D06: the extra outputs as SINKS (they are low at rest), one LED column pulsed high: 0, 1, 2
    for (uint8_t k = 0; k < 3; k++) {
        type_label('D', (uint8_t)(4 + k));
        pulse_column(k);
    }
    // E00-E15: column 1 high, strobe 0..15 low; E16-E31: the same with column 2
    for (uint8_t k = 1; k <= 2; k++) {
        for (uint8_t sink = 0; sink < 16; sink++) {
            type_label('E', (uint8_t)((k - 1) * 16 + sink));
            light(k, sink);
        }
    }
    // F00-F05: PWM, DUTY1 = 198, DUTY2 = DUTY1 + width. F06-F11: DUTY2 = 198, DUTY1 = DUTY2 + width.
    static const __code uint16_t widths[6] = { 0, 30, 120, 300, 600, 1100 };
    for (uint8_t i = 0; i < 6; i++) {
        type_label('F', i);
        pwm_step(198, (uint16_t)(198 + widths[i]));
    }
    for (uint8_t i = 0; i < 6; i++) {
        type_label('F', (uint8_t)(6 + i));
        pwm_step((uint16_t)(198 + widths[i]), 198);
    }
#endif

    user_matrix_cols_deselect_all();
    tick_resume();
}

void kb_update(void)
{
    static bool done;

    if (!done && timer_read() >= START_MS) {
        done = true;
        run_led_test();
    }
}
