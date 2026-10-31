#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "kbdef.h"

// Keyboard HUD protocol: lets a desktop overlay show the keyboard's live state (layers, pressed keys,
// modifiers, lock LEDs). It rides on the VIA/Vial raw HID interface as command 0xF0 and only ever
// answers requests: nothing is sent unasked and nothing is stored. Adapted from the QMK module in tools/hud/.
//
// A board enables it with `#define HUD_ENABLE` in kbdef.h (it needs the raw HID build).
//
// Wire format, every packet 32 bytes: [0xF0, command, status, sequence, args... / reply...]
//   0x01 GET_INFO    protocol, rows, cols, layers, capability bits, HUD_TOG keycode, keymap version
//   0x02 GET_STATE   layer mask, default layer, pressed keys (low 8 columns), mods, one-shots, flags, keymap version
//   0x03 GET_KEYMAP  args: layer, offset (16 bit), count (<= 12); reply: effective keycodes
//   0x04 GET_MATRIX  (protocol 2, matrices wider than 8) the whole pressed-key matrix, rows * ceil(cols / 8) bytes
//
// Protocol 2 differs from 1 only for a keyboard with more than 8 columns: GET_INFO carries the bytes per row,
// the pressed-key bytes of GET_STATE hold just the first 8 columns (so a protocol 1 reader stays correct for those),
// and GET_MATRIX returns the full matrix.

#ifdef HUD_ENABLE

#define HUD_CMD 0xF0

// Handles a HUD request in `data` (32 bytes) and turns it into the reply. False if `data` is not a HUD packet.
bool hud_receive(uint8_t *data, uint8_t length);

// The HUD_TOG key went down: the app sees the counter change.
void hud_toggle_pressed(void);

// The keycode of the board's HUD_TOG key (0 if none).
uint16_t hud_toggle_keycode(void);

#endif
