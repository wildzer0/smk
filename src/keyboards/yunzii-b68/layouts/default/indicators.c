#include "indicators.h"
#include "kbdef.h"
#include "pwm.h"
#include "settings.h"
#include "tick.h"
#include "keyboard.h"
#include "led_effect.h"
#include "user_led.h"
#include "user_matrix.h"
#include <string.h>

// RGB backlight of the Yunzii B68, as found with the LED probe (docs/keyboards/yunzii-b68.md).
//
// Every key has an RGB LED. One LED lights when its colour channel (a PWM pin on P1/P2/P3) is high and the key matrix strobe of its
// COLUMN is low, so the strobes double as the LED sinks and all the keys of one column (five rows x R, G, B = 15 channels) are lit by
// one subframe. The frame is 16 subframes, one per column, interleaved with the key scans by the tick.
//
// The hardware PWM makes its output high between DUTY1 and DUTY2 (checked with the probe: the width is the brightness, whichever
// register is the larger). DUTY1 is fixed per channel, staggered like the stock firmware does so the channels do not all switch at once;
// DUTY2 = DUTY1 + width.

#define LED_ROWS MATRIX_ROWS
#define LED_COLS MATRIX_COLS

#define LED_SPEED_DEFAULT      4
#define LED_SPEED_MIN          1
#define LED_SPEED_MAX          16
#define LED_BRIGHTNESS_DEFAULT 160
#define LED_BRIGHTNESS_STEP    32

#define PWM_PERIOD_H 0x04 // 1200 counts, as the stock firmware
#define PWM_PERIOD_L 0xB0

#define PWM_DUTY1_BASE 195u // the stock firmware's first DUTY1 (0xC3); channel n uses 195 + n
#define PWM_BANK_ON    0x89 // the first channel of a bank starts the bank
#define PWM_CHANNEL_ON 0x08

// 8-bit level -> pulse width in counts: 3.5 per step, 892 at most, so DUTY2 stays below the 1200-count period
#define LED_WIDTH(v) ((uint16_t)((uint16_t)(v) * 3u + ((uint16_t)(v) >> 1)))

#include LED_GEOMETRY_HEADER
_Static_assert(LED_GEOMETRY_ROWS == LED_ROWS && LED_GEOMETRY_COLS == LED_COLS, "generated LED geometry size does not match the key matrix");

// The 15 colour channels, in the order of the framebuffer: row 0 R G B, row 1 R G B, ...  X(n, pwm channel)
#define LED_CHANNELS(X)                                                                                 \
    X(0, PWM23) X(1, PWM24) X(2, PWM25)    /* row 0: P1.3 P1.4 P1.5 */                                  \
    X(3, PWM10) X(4, PWM11) X(5, PWM12)    /* row 1: P2.0 P2.1 P2.2 */                                  \
    X(6, PWM13) X(7, PWM14) X(8, PWM15)    /* row 2: P2.3 P2.4 P2.5 */                                  \
    X(9, PWM03) X(10, PWM04) X(11, PWM05)  /* row 3: P3.3 P3.4 P3.5 (the shift row) */                  \
    X(12, PWM00) X(13, PWM01) X(14, PWM02) /* row 4: P3.0 P3.1 P3.2 (the bottom row) */

static __xdata uint8_t led_fb[LED_COLS][LED_ROWS * 3]; // [column][row * 3 + colour]

static uint8_t led_col;   // the next column to light
static uint8_t led_phase; // effect animation phase
static uint8_t anim_ctr;  // frames since the last phase step
static uint8_t regen_row;
static uint8_t regen_col;

static volatile bool render_dirty;

// Driver experiments, flipped with Fn+A, S, D (see kbdef.h): bit 0 the bank of row 0 is started by its own first channel instead of
// PWM20, bit 1 the PWM keeps running between columns (only the widths and the sink change), bit 2 the animation is frozen
static uint8_t tests;
#define TEST_NO_PWM20   (1u << 0)
#define TEST_KEEP_PWM   (1u << 1)
#define TEST_FREEZE     (1u << 2)

void indicators_apply_defaults()
{
    user_settings.led_effect     = FX_RADIAL;
    user_settings.led_brightness = LED_BRIGHTNESS_DEFAULT;
    user_settings.led_speed      = LED_SPEED_DEFAULT;
#ifdef RF_ENABLED
    user_settings.rf_link = 0;
#endif
}

void indicators_validate_settings()
{
    if (user_settings.led_effect > FX_OFF) {
        user_settings.led_effect = FX_OFF;
    }
    if (user_settings.led_speed < LED_SPEED_MIN) {
        user_settings.led_speed = LED_SPEED_MIN;
    }
    if (user_settings.led_speed > LED_SPEED_MAX) {
        user_settings.led_speed = LED_SPEED_MAX;
    }
}

void indicators_pwm_enable()
{
    PWM00CON = PWM_BANK_ON;
    PWM01CON = PWM_CHANNEL_ON;
    PWM02CON = PWM_CHANNEL_ON;
    PWM03CON = PWM_CHANNEL_ON;
    PWM04CON = PWM_CHANNEL_ON;
    PWM05CON = PWM_CHANNEL_ON;

    PWM10CON = PWM_BANK_ON;
    PWM11CON = PWM_CHANNEL_ON;
    PWM12CON = PWM_CHANNEL_ON;
    PWM13CON = PWM_CHANNEL_ON;
    PWM14CON = PWM_CHANNEL_ON;
    PWM15CON = PWM_CHANNEL_ON;

    // P1.0-P1.2 are not LED channels: the bank is started by its first channel, whose width is kept at zero
    if (tests & TEST_NO_PWM20) {
        PWM20CON = 0;
        PWM23CON = PWM_BANK_ON;
    } else {
        PWM20CON = PWM_BANK_ON;
        PWM23CON = PWM_CHANNEL_ON;
    }
    PWM24CON = PWM_CHANNEL_ON;
    PWM25CON = PWM_CHANNEL_ON;
}

// Parked: the pins fall back to their latches (low), so nothing is driven.
void indicators_pwm_disable()
{
    PWM00CON = 0;
    PWM01CON = 0;
    PWM02CON = 0;
    PWM03CON = 0;
    PWM04CON = 0;
    PWM05CON = 0;
    PWM10CON = 0;
    PWM11CON = 0;
    PWM12CON = 0;
    PWM13CON = 0;
    PWM14CON = 0;
    PWM15CON = 0;
    PWM20CON = 0;
    PWM23CON = 0;
    PWM24CON = 0;
    PWM25CON = 0;
}

void indicators_init()
{
    memset(led_fb, 0, sizeof(led_fb));

    PWM0PERDH = PWM_PERIOD_H;
    PWM0PERDL = PWM_PERIOD_L;
    PWM1PERDH = PWM_PERIOD_H;
    PWM1PERDL = PWM_PERIOD_L;
    PWM2PERDH = PWM_PERIOD_H;
    PWM2PERDL = PWM_PERIOD_L;

#define INIT_CHANNEL(n, pwm) SET_PWM_DUTY(pwm, (uint16_t)(PWM_DUTY1_BASE + (n)), (uint16_t)(PWM_DUTY1_BASE + (n)));
    LED_CHANNELS(INIT_CHANNEL)
#undef INIT_CHANNEL

    // the unused first channel of bank 2 stays at zero width
    SET_PWM_DUTY(PWM20, (uint16_t)PWM_DUTY1_BASE, (uint16_t)PWM_DUTY1_BASE);
}

void indicators_start()
{
    led_col      = 0;
    anim_ctr     = 0;
    led_phase    = 0;
    regen_row    = 0;
    regen_col    = 0;
    render_dirty = true;

    indicators_pwm_enable();
}

// Called by the tick before every LED subframe and key scan: the previous subframe's sink goes off.
void indicators_pre_update()
{
    user_matrix_cols_deselect_all();
}

void indicators_post_update()
{
    PWM00CON &= ~(1 << 5);
}

// Loads the 15 widths of column `col`; false if the column is entirely dark.
static bool load_column(uint8_t col)
{
    __xdata uint8_t *fb = led_fb[col];

    uint8_t any = 0;

#define LOAD_CHANNEL(n, pwm)                                                                                  \
    SET_PWM_DUTY_2(pwm, (uint16_t)(PWM_DUTY1_BASE + (n) + LED_WIDTH(fb[n])));                                \
    any = (uint8_t)(any | fb[n]);
    LED_CHANNELS(LOAD_CHANNEL)
#undef LOAD_CHANNEL

    return any != 0;
}

bool indicators_update_step(keyboard_state_t *keyboard, uint8_t current_step)
{
    keyboard;
    current_step;

    if (tests & TEST_KEEP_PWM) {
        if (load_column(led_col)) {
            user_matrix_col_select(led_col);
        }
    } else {
        indicators_pwm_disable(); // park while the widths change

        if (load_column(led_col)) {
            user_matrix_col_select(led_col); // this column's keys are the ones lit
            indicators_pwm_enable();         // enabled last: the column lights now
        }
    }

    bool frame_wrapped = false;
    if (++led_col >= LED_COLS) {
        led_col       = 0;
        frame_wrapped = true;

        if (++anim_ctr >= 4) {
            anim_ctr = 0;
        }
        if (anim_ctr == 0 && !(tests & TEST_FREEZE)) {
            led_phase = (uint8_t)(led_phase + user_settings.led_speed);
            render_dirty = true;
        }
    }

    return frame_wrapped;
}

static void regen_one(void)
{
    uint8_t rgb[3] = { 0, 0, 0 };

    if (!led_effect_rgb((led_effect_t)user_settings.led_effect, regen_row, regen_col, led_phase, user_settings.led_brightness, rgb)) {
        rgb[0] = rgb[1] = rgb[2] = 0; // effect off
    }

    // Caps Lock: the Caps key (row 2, column 0) is a solid white while it is on
    if (regen_row == 2 && regen_col == 0 && (keyboard_state.led_state & (1u << 1))) {
        rgb[0] = rgb[1] = rgb[2] = 255;
    }

    led_fb[regen_col][regen_row * 3 + 0] = rgb[0];
    led_fb[regen_col][regen_row * 3 + 1] = rgb[1];
    led_fb[regen_col][regen_row * 3 + 2] = rgb[2];

    if (++regen_col >= LED_COLS) {
        regen_col = 0;
        if (++regen_row >= LED_ROWS) {
            regen_row = 0;
        }
    }
}

void indicators_render()
{
    // Caps Lock changes outside the animation: repaint when it does
    static uint8_t last_caps;
    const uint8_t  caps = (uint8_t)(keyboard_state.led_state & (1u << 1));

    if (caps != last_caps) {
        last_caps    = caps;
        render_dirty = true;
    }

    if (!render_dirty) {
        return;
    }
    render_dirty = false; // cleared first: a phase step during the repaint asks for another

    for (uint8_t i = 0; i < (uint8_t)(LED_ROWS * LED_COLS); i++) {
        regen_one();
    }
}

// ---- keys: effect, brightness, speed -------------------------------------------------------------------------

void indicators_next_effect()
{
    if (++user_settings.led_effect > FX_OFF) {
        user_settings.led_effect = 0;
    }
    settings_mark_dirty();
    render_dirty = true;
}

void indicators_prev_effect()
{
    user_settings.led_effect = (user_settings.led_effect == 0) ? FX_OFF : (uint8_t)(user_settings.led_effect - 1);
    settings_mark_dirty();
    render_dirty = true;
}

void indicators_brightness_up()
{
    user_settings.led_brightness = (user_settings.led_brightness > (uint8_t)(255 - LED_BRIGHTNESS_STEP)) ? 255 : (uint8_t)(user_settings.led_brightness + LED_BRIGHTNESS_STEP);
    settings_mark_dirty();
    render_dirty = true;
}

void indicators_brightness_down()
{
    user_settings.led_brightness = (user_settings.led_brightness < LED_BRIGHTNESS_STEP) ? 0 : (uint8_t)(user_settings.led_brightness - LED_BRIGHTNESS_STEP);
    settings_mark_dirty();
    render_dirty = true;
}

void indicators_speed_up()
{
    if (user_settings.led_speed < LED_SPEED_MAX) {
        user_settings.led_speed++;
    }
    settings_mark_dirty();
}

void indicators_speed_down()
{
    if (user_settings.led_speed > LED_SPEED_MIN) {
        user_settings.led_speed--;
    }
    settings_mark_dirty();
}

void indicators_toggle_test(uint8_t bit)
{
    indicators_pwm_disable();
    tests ^= (uint8_t)(1u << bit);
    indicators_pwm_enable();
}

// A flash write runs with interrupts off for ~5 ms: park the scan and the LEDs across it.
void settings_save_pre(void)
{
    tick_pause();
    indicators_pwm_disable();
    user_matrix_cols_deselect_all();
}

void settings_save_post(void)
{
    indicators_pwm_enable();
    tick_resume();
}
