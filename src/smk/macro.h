#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "kbdef.h"

// Vial macros: a macro key (M0, M1, ...) types a stored sequence. A board with a macro buffer defines
//
//   #define KEYMAP_MACRO_COUNT 16   // how many macros Vial offers
//   #define KEYMAP_MACRO_BYTES 512  // size of the buffer holding all of them
//
// in kbdef.h. The buffer is the one VIA/Vial read and write (macros back to back, each ended by a 0
// byte): plain characters are typed as on a US keyboard, and 0x01 introduces an action - tap / down
// / up of a key, a delay, or the same with a 16-bit keycode. A macro runs to its end before the main
// loop goes on, as in QMK.

#ifdef KEYMAP_MACRO_BYTES
void macro_play(uint8_t id);

// Provided by the caller.
void macro_hook_press(uint16_t keycode);
void macro_hook_release(uint16_t keycode);
void macro_hook_wait_ms(uint16_t ms);
#endif
