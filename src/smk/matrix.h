#pragma once

#include <stdint.h>
#include <stdbool.h>

void    matrix_init();
uint8_t matrix_task();

void matrix_scan_full();

bool matrix_any_pressed(void);

// True while the key at (row, col) is down, as of the last pass of matrix_task().
bool matrix_is_on(uint8_t row, uint8_t col);

// Press and release a keycode right away (an encoder step). Layer keycodes are not supported.
void matrix_tap_keycode(uint16_t keycode);
