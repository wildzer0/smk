#pragma once

// Recovery chord: a board that defines RECOVERY_CHORD_KEYS in kbdef.h gets a check at
// power-on that jumps to the ISP bootloader when those matrix cells are held. It runs
// right after user_init(), before USB, the radio and the LEDs come up, so it still
// works when a later part of the firmware misbehaves.
//
// kbdef.h:
//   #define RECOVERY_CHORD_COUNT 3
//   #define RECOVERY_CHORD_KEYS  { { col, row }, { col, row }, { col, row } }
#include "kbdef.h"

#ifdef RECOVERY_CHORD_KEYS
void recovery_check(void);
#else
#    define recovery_check() ((void)0)
#endif
