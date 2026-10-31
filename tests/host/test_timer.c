// Host unit tests for src/smk/timer.c: sweeps are turned into milliseconds, and the sweep
// length is re-measured against the USB start-of-frame (1 ms) so it follows the real tick.

#include "timer.h"
#include <stdio.h>

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

// Runs `real_ms` of real time on a board whose sweep lasts `sweep_us`, delivering sweeps and
// (if `usb`) one SOF per millisecond, in time order.
static void run(uint32_t real_ms, uint32_t sweep_us, int usb)
{
    static uint32_t t_us;  // time of the next event, absolute
    static uint32_t next_sweep;
    static uint32_t next_sof;

    if (next_sweep == 0) {
        next_sweep = sweep_us;
        next_sof   = 1000;
    }

    const uint32_t end = t_us + real_ms * 1000u;
    while (1) {
        const uint32_t sweep_at = next_sweep;
        const uint32_t sof_at   = usb ? next_sof : 0xFFFFFFFFu;
        const uint32_t at       = sweep_at < sof_at ? sweep_at : sof_at;

        if (at > end) {
            break;
        }
        if (sweep_at <= sof_at) {
            timer_sweep();
            next_sweep += sweep_us;
        } else {
            timer_sof();
            next_sof += 1000;
        }
    }
    t_us = end;
    if (!usb) {
        next_sof = ((end / 1000u) + 1u) * 1000u;
    }
}

static int near(uint32_t value, uint32_t want, uint32_t tolerance)
{
    return value + tolerance >= want && value <= want + tolerance;
}

int main(void)
{
    // no USB yet: the default sweep length (750 us) is used. A board that really takes 900 us
    // per sweep runs the clock 17% slow until the first measurement.
    run(1000, 900, 0);
    CHECK(near(timer_read32(), 833, 8));

    // on USB the first measurement lands after 256 ms
    run(300, 900, 1);
    CHECK(timer_sweep_us() >= 895 && timer_sweep_us() <= 905);

    // from then on the clock is right within half a percent
    const uint32_t t0 = timer_read32();
    run(10000, 900, 1);
    CHECK(near(timer_read32() - t0, 10000, 50));

    // unplugged: it keeps the measured length, so it stays accurate
    const uint32_t t1 = timer_read32();
    run(10000, 900, 0);
    CHECK(near(timer_read32() - t1, 10000, 50));

    // a different board speed is picked up
    run(600, 400, 1);
    CHECK(timer_sweep_us() >= 395 && timer_sweep_us() <= 405);

    // the 16-bit view and the elapsed helper wrap correctly
    const uint16_t before = timer_read();
    run(70000, 400, 1);
    CHECK(near(timer_elapsed(before), (uint16_t)70000u, 80));

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
