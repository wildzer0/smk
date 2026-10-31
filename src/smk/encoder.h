#pragma once

#include <stdint.h>
#include "kbdef.h"

// Rotary encoders. A board with one defines in kbdef.h:
//
//   #define ENCODER_COUNT 1
//   #define ENCODER_RESOLUTION 4        // pin changes per detent
//   #define ENCODER_READ_STATE(n) ...   // 2 bits for encoder n: bit 0 = A, bit 1 = B
//
// and its layout supplies `encoder_map[layer][encoder][2]` (counter-clockwise, clockwise), the way
// QMK's ENCODER_MAP does. A step is looked up through the active layers like any key and then tapped.

#ifdef ENCODER_COUNT
// The tick calls this on every timer interrupt: it only decodes the pins and counts steps.
void encoder_sample(void);

// Main loop: turns the counted steps into key taps.
void encoder_task(void);
#else
#    define encoder_sample() ((void)0)
#    define encoder_task()   ((void)0)
#endif
