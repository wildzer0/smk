#pragma once

#include "sh68f90.h"
#include "keycodes.h"
#include <stdint.h>
#include <stdbool.h>

// Yunzii B68 (BYK916 = SH68F90A + BK3632).
//
// The pins below were read from the stock firmware (docs/keyboards/yunzii-b68-firmware-stock.md,
// "Matrice") and confirmed on the board with the probe layout: every key answered on the cell expected
// (docs/keyboards/yunzii-b68.md). P7.0 and the LED/radio pins are still unidentified.

#define MATRIX_ROWS 5
#define MATRIX_COLS 16

// The keymap is editable at runtime: 8 layers kept in RAM and stored in flash (the
// `keymap_sectors` option in meson.build reserves the room; keymap_store.c checks it fits).
#define KEYMAP_DYNAMIC_LAYERS 8

// Vial macros: 16 of them in a 512-byte buffer, stored with the keymap
#define KEYMAP_MACRO_COUNT 16
#define KEYMAP_MACRO_BYTES 512

// Row Pin Bits
#define KB_R0_P7_1 _P7_1
#define KB_R1_P7_2 _P7_2
#define KB_R2_P7_3 _P7_3
#define KB_R3_P5_3 _P5_3
#define KB_R4_P5_4 _P5_4

// Row Pins (inputs with pull-up, pressed = 0)
#define KB_R0 P7_1
#define KB_R1 P7_2
#define KB_R2 P7_3
#define KB_R3 P5_3
#define KB_R4 P5_4

// P7.0: an input the stock firmware reads together with the rows, function unknown.
// It is NOT part of the matrix.

// Column Pin Bits
#define KB_C0_P6_0  _P6_0
#define KB_C1_P6_1  _P6_1
#define KB_C2_P6_2  _P6_2
#define KB_C3_P6_3  _P6_3
#define KB_C4_P6_4  _P6_4
#define KB_C5_P6_5  _P6_5
#define KB_C6_P6_6  _P6_6
#define KB_C7_P6_7  _P6_7
#define KB_C8_P5_0  _P5_0
#define KB_C9_P5_1  _P5_1
#define KB_C10_P5_2 _P5_2
#define KB_C11_P5_7 _P5_7
#define KB_C12_P4_0 _P4_0
#define KB_C13_P4_2 _P4_2
#define KB_C14_P4_3 _P4_3
#define KB_C15_P4_5 _P4_5

// Column Pins (strobes, active low)
#define KB_C0  P6_0
#define KB_C1  P6_1
#define KB_C2  P6_2
#define KB_C3  P6_3
#define KB_C4  P6_4
#define KB_C5  P6_5
#define KB_C6  P6_6
#define KB_C7  P6_7
#define KB_C8  P5_0
#define KB_C9  P5_1
#define KB_C10 P5_2
#define KB_C11 P5_7
#define KB_C12 P4_0
#define KB_C13 P4_2
#define KB_C14 P4_3
#define KB_C15 P4_5

// Recovery chord (see src/smk/recovery.h): hold Esc + 1 + 2 (cells c0r0, c1r0, c2r0, mapped with the probe
// layout) while plugging in to jump to the ISP bootloader. All three must stay down for ~60 ms.
#define RECOVERY_CHORD_COUNT 3
#define RECOVERY_CHORD_KEYS  { { 0u, 0u }, { 1u, 0u }, { 2u, 0u } }

// Rotary encoder: A on P0.6, B on P0.7 (found with the probe layout). The pins are inputs with
// pull-ups, and a detent is one full cycle of the two signals (4 pin changes).
#define ENCODER_COUNT 1
#define ENCODER_RESOLUTION 4
#define ENCODER_READ_STATE(n) ((uint8_t)((P0 >> 6) & 0x03u))

// Keyboard HUD (tools/hud): live state over the raw HID interface, plus a key to show/hide the overlay.
// HUD_TOG is the first custom keycode (QK_KB_0, listed in vial.json under customKeycodes).
#define HUD_ENABLE
#define HUD_TOGGLE_KEYCODE B68_HUD_TOG

// Custom keycodes (the first QK_KB values, listed in the same order under customKeycodes in vial.json)
#define B68_HUD_TOG 0x7E00
#define B68_FX_PREV 0x7E01
#define B68_FX_NEXT 0x7E02
#define B68_BRI_DN  0x7E03
#define B68_BRI_UP  0x7E04
#define B68_SPD_DN  0x7E05
#define B68_SPD_UP  0x7E06

// Two LED subframes per key scan: the scan takes about 320 us and a subframe lights one column for 400 us, so a frame of 16 columns
// refreshes at roughly 55 Hz.
#define LED_SUBFRAMES_PER_SCAN 2
