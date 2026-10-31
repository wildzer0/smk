#pragma once

#include <stdint.h>
#include <stdbool.h>

// Layer engine: a bitmask of active layers on top of a default layer, the layer
// keycodes (MO, TO, TG, DF, OSL) and one-shot modifiers (OSM).
//
// A key is looked up in the highest active layer that does not hold KC_TRANSPARENT; the
// default layer is the last one tried. The keycode found at press time is the one used at
// release (layer_key_down/up cache it), so a layer change between press and release can never
// leave a key stuck.

#define LAYER_MAX 8 // layers are bits of a uint8_t

// Provided by the firmware (keymap.c) or by a test.
uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col);
uint8_t  keymap_layer_count(void);

// Provided by the caller: apply/remove modifier bits (HID modifier byte) on the host report.
void layer_hook_mods_add(uint8_t mods);
void layer_hook_mods_del(uint8_t mods);

void    layer_init(void);
void    layer_set_default(uint8_t layer);
uint8_t layer_get_default(void);

// The keycode under the active layers for a key about to be pressed.
uint16_t layer_lookup(uint8_t row, uint8_t col);

// A key went down / up: the keycode to act on. Down resolves through the layers and
// remembers the result; up returns that same keycode, whatever the layers say by then.
uint16_t layer_key_down(uint8_t row, uint8_t col);
uint16_t layer_key_up(uint8_t row, uint8_t col);

// Layer and one-shot keycodes. Returns true when `keycode` was one of them (and handled).
bool layer_process(uint16_t keycode, bool pressed);

// Every other key calls these around its press and release, so one-shots get used up.
void layer_key_pressed(void);
void layer_key_released(void);
