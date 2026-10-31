#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "kbdef.h"

// Provided by the board's layout.c next to `keymaps`: how many layers it defines.
extern const uint8_t keymap_layers;

// A board whose build reserves flash for the keymap (the `keymap_sectors` board option, which
// defines FLASH_KEYMAP_ADDR) has an editable keymap. The others read the compiled-in one and
// these functions do nothing.
void keymap_init(void);

// Edit one key. False if the board's keymap is fixed or the position does not exist.
bool keymap_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode);

// Back to the compiled-in keymap.
void keymap_reset(void);

// Writes pending edits to flash once the keyboard has been quiet for a while.
void keymap_task(void);

#ifdef ENCODER_COUNT
// Edit one rotation keycode of an encoder (clockwise or counter-clockwise).
bool keymap_encoder_set(uint8_t layer, uint8_t encoder, bool clockwise, uint16_t keycode);
#endif
