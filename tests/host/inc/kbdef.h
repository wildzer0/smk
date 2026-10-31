#pragma once
#include <stdint.h>
#include <stdbool.h>
// Stand-in board for the host unit tests: 2 rows x 3 columns.
#ifndef MATRIX_ROWS
#    define MATRIX_ROWS 2
#endif
#ifndef MATRIX_COLS
#    define MATRIX_COLS 3
#endif

// keymap_store.c: 8 layers of this 2x3 board plus its encoder is 128 B, so a copy needs four 64 B sectors (macros included).
#define KEYMAP_DYNAMIC_LAYERS       8
#define FLASH_KEYMAP_ADDR           0x1000u
#define FLASH_KEYMAP_SECTORS        4u
#define FLASH_KEYMAP_SECTOR_SIZE    64u

// encoder.c / layer.c: one encoder whose pins are a test variable
#define ENCODER_COUNT 1
#define ENCODER_RESOLUTION 4
extern uint8_t test_encoder_state;
#define ENCODER_READ_STATE(n) (test_encoder_state)

// macro.c / via.c: four macros in 64 bytes
#define KEYMAP_MACRO_COUNT 4
#define KEYMAP_MACRO_BYTES 64

// hud.c
#define HUD_ENABLE
#define HUD_TOGGLE_KEYCODE 0x7E00
