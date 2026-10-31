#pragma once
#include <stdint.h>
#include <stdbool.h>
// Stand-in board for the host unit tests: 2 rows x 3 columns.
#define MATRIX_ROWS 2
#define MATRIX_COLS 3

// keymap_store.c: 8 layers of this 2x3 board is 96 B, so a copy needs two 64 B sectors.
#define KEYMAP_DYNAMIC_LAYERS       8
#define FLASH_KEYMAP_ADDR           0x1000u
#define FLASH_KEYMAP_SECTORS        2u
#define FLASH_KEYMAP_SECTOR_SIZE    64u

// encoder.c / layer.c: one encoder whose pins are a test variable
#define ENCODER_COUNT 1
#define ENCODER_RESOLUTION 4
extern uint8_t test_encoder_state;
#define ENCODER_READ_STATE(n) (test_encoder_state)
