#pragma once

#include <stdint.h>
#include <stdbool.h>

// The runtime-editable keymap: a RAM copy of every layer (KEYMAP_DYNAMIC_LAYERS of them),
// persisted in two flash slots (A/B) of FLASH_KEYMAP_SECTORS sectors each.
//
// A save erases the slot that is NOT in use, writes the payload, and writes the header last.
// The header carries a sequence number and a CRC of the payload, so a slot is only valid once
// it is complete; at boot the valid slot with the newest sequence wins. A power cut during a
// save therefore leaves the previous keymap intact.

// Provided by the caller: the compiled-in keymap used when flash holds nothing valid.
// Layers the build does not define must answer KC_TRANSPARENT.
uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col);

void keymap_store_init(void);

uint16_t keymap_store_get(uint8_t layer, uint8_t row, uint8_t col);

// Changes the RAM copy and marks it dirty. False for a position that does not exist.
bool keymap_store_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode);

// Back to the compiled-in keymap (dirty, so the next flush stores it).
void keymap_store_reset(void);

bool keymap_store_dirty(void);

// Writes the RAM copy to flash if it changed. Takes tens of milliseconds.
void keymap_store_flush(void);
