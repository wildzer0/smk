#include "recovery.h"

#ifdef RECOVERY_CHORD_KEYS

#include "isp.h"
#include "delay.h"
#include "watchdog.h"
#include "user_matrix.h"
#include <stdbool.h>
#include <stdint.h>

// held, not merely seen: a chord sampled once could be one scan of a key being released
#define RECOVERY_SAMPLES   6u
#define RECOVERY_SAMPLE_MS 10u

static uint8_t sample[MATRIX_COLS];

// Same scan as matrix_scan_full(), without the LED PWM handling: nothing else is up yet.
static void scan_matrix(void)
{
    user_matrix_sinks_off();
    user_matrix_scan_pre();
    user_matrix_cols_deselect_all();

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        user_matrix_col_select(col);

        delay_us(10); // let the row lines settle before sampling
        const uint8_t first = user_matrix_read_rows();
        delay_us(10);
        const uint8_t second = user_matrix_read_rows();

        sample[col] = (first == second) ? (uint8_t)~first : 0u;

        user_matrix_col_deselect(col);
    }

    user_matrix_scan_post();
}

static bool chord_down(void)
{
    static const __code uint8_t chord[RECOVERY_CHORD_COUNT][2] = RECOVERY_CHORD_KEYS;

    for (uint8_t key = 0; key < RECOVERY_CHORD_COUNT; key++) {
        if (!(sample[chord[key][0]] & (uint8_t)(1u << chord[key][1]))) {
            return false;
        }
    }

    return true;
}

_Static_assert(RECOVERY_CHORD_COUNT > 0, "an empty recovery chord would trigger on every boot");

void recovery_check(void)
{
    for (uint8_t i = 0; i < RECOVERY_SAMPLES; i++) {
        watchdog_kick();
        scan_matrix();

        if (!chord_down()) {
            return;
        }

        delay_ms(RECOVERY_SAMPLE_MS);
    }

    isp_jump();
}

#endif // RECOVERY_CHORD_KEYS
