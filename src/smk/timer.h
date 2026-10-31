#pragma once

#include <stdint.h>

// A millisecond clock for things that need real time (the Vial unlock, tapping terms).
//
// The only free-running source on this part is the Timer2 tick, which alternates between
// matrix sweeps and LED subframes, so a sweep's length depends on the board. The clock counts
// sweeps and turns them into milliseconds with a measured length of a sweep. While the host
// is driving USB, every 256 start-of-frame packets (1 ms each) the length is measured again,
// so the clock is accurate to a fraction of a percent on USB and keeps the last measurement
// when there is no USB.

uint16_t timer_read(void);                    // milliseconds, wraps at 65536
uint32_t timer_read32(void);                  // milliseconds since boot
uint16_t timer_elapsed(uint16_t since);       // milliseconds since a timer_read() value

void timer_sweep(void); // from the tick, once per matrix sweep
void timer_sof(void);   // from the USB interrupt, once per start-of-frame

// For tests: the sweep length currently in use, in microseconds.
uint16_t timer_sweep_us(void);
