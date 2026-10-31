#include "timer.h"

// The start value until the first measurement: a sweep plus an LED subframe, as seen in the simulator.
#define SWEEP_US_DEFAULT 750u

// A measurement outside this range is a glitch (a flash write, a bus reset), not a sweep length.
#define SWEEP_US_MIN 200u
#define SWEEP_US_MAX 4000u

#define SOF_WINDOW 256u // SOFs per measurement: 256 ms

static volatile uint32_t now_ms;
static uint16_t          sweep_us = SWEEP_US_DEFAULT;
static uint16_t          carry_us; // microseconds counted but not yet worth a millisecond

// Sweeps are counted for good, and a measurement window remembers where it started.
static volatile uint16_t sweep_count;
static uint16_t          window_start; // sweep_count at the first SOF of the window
static uint16_t          last_sof_at;  // sweep_count at the previous SOF
static uint16_t          window_sofs;

// More sweeps than this between two SOFs (1 ms apart) means SOFs were not arriving: USB was
// unplugged, suspended or just plugged in, so the window in progress means nothing.
#define MAX_SWEEPS_PER_SOF 40u

void timer_sweep(void)
{
    sweep_count++;

    carry_us += sweep_us;
    while (carry_us >= 1000u) {
        carry_us -= 1000u;
        now_ms++;
    }
}

static uint16_t sweeps_now(void)
{
    uint16_t a;
    uint16_t b;

    do {
        a = sweep_count;
        b = sweep_count;
    } while (a != b);

    return a;
}

void timer_sof(void)
{
    const uint16_t now_sweeps = sweeps_now();
    const uint16_t gap        = (uint16_t)(now_sweeps - last_sof_at);

    last_sof_at = now_sweeps;

    if (gap > MAX_SWEEPS_PER_SOF) {
        window_sofs = 0; // a new window starts with the next SOF
        return;
    }

    if (window_sofs == 0) {
        window_start = now_sweeps;
    }

    if (++window_sofs < SOF_WINDOW + 1u) {
        return;
    }

    // SOF_WINDOW milliseconds passed between the first and the last SOF of the window
    const uint16_t sweeps = (uint16_t)(now_sweeps - window_start);
    window_sofs           = 0;

    if (sweeps == 0) {
        return;
    }

    const uint32_t us = (uint32_t)SOF_WINDOW * 1000u / sweeps;
    if (us >= SWEEP_US_MIN && us <= SWEEP_US_MAX) {
        sweep_us = (uint16_t)us;
    }
}

uint32_t timer_read32(void)
{
    uint32_t a;
    uint32_t b;

    // 32 bits are not read atomically: take two readings until they agree
    do {
        a = now_ms;
        b = now_ms;
    } while (a != b);

    return a;
}

uint16_t timer_read(void)
{
    return (uint16_t)timer_read32();
}

uint16_t timer_elapsed(uint16_t since)
{
    return (uint16_t)(timer_read() - since);
}

uint16_t timer_sweep_us(void)
{
    return sweep_us;
}
