#pragma once

// The keyboard's realtime work, driven by the platform's periodic tick.
//
// One hardware timer has to serve two jobs that can't overlap: sweeping the key
// matrix, and emitting LED PWM subframes (LED drive current couples into the row
// sense). So ticks alternate between them, and this is where that interleave is
// decided.

#include <stdint.h>

void tick_init(void);

void tick_dispatch(void);

void tick_pause(void);
void tick_resume(void);

// Matrix sweeps since boot, wrapping at 65536. A coarse clock for things that only need "a
// while": a sweep takes around a millisecond, depending on the board's LED subframe count.
// Differences of two readings are wrap-safe.
uint16_t tick_sweeps(void);
