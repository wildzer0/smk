#pragma once

#include <stdint.h>
#include <stdbool.h>

void    matrix_init();
uint8_t matrix_task();

void matrix_scan_full();

bool matrix_any_pressed(void);
