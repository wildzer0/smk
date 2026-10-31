#include "kbdef.h"
#include "gpio.h"
#include "user_matrix.h"

// 16 columns are the strobes (active low), 5 rows are inputs with pull-up.
#define KB_C_P4_MASK (uint8_t)(KB_C12_P4_0 | KB_C13_P4_2 | KB_C14_P4_3 | KB_C15_P4_5)
#define KB_C_P5_MASK (uint8_t)(KB_C8_P5_0 | KB_C9_P5_1 | KB_C10_P5_2 | KB_C11_P5_7)
#define KB_C_P6_MASK (uint8_t)(KB_C0_P6_0 | KB_C1_P6_1 | KB_C2_P6_2 | KB_C3_P6_3 | KB_C4_P6_4 | KB_C5_P6_5 | KB_C6_P6_6 | KB_C7_P6_7)

void user_matrix_cols_deselect_all(void)
{
    GPIO_HIGH(4, KB_C_P4_MASK);
    GPIO_HIGH(5, KB_C_P5_MASK);
    GPIO_HIGH(6, KB_C_P6_MASK);
}

// The columns stay outputs at all times (user_init sets them up) and there are no
// LEDs sharing the lines yet, so nothing to do before/after a scan.
void user_matrix_scan_pre(void) {}

void user_matrix_scan_post(void) {}

// TODO(M5): drive the LED sink lines low during the scan once the LEDs exist.
void user_matrix_sinks_off(void) {}

void user_matrix_col_select(uint8_t col) // active-low: drive LOW
{
    switch (col) {
        case 0:
            KB_C0 = 0;
            break;
        case 1:
            KB_C1 = 0;
            break;
        case 2:
            KB_C2 = 0;
            break;
        case 3:
            KB_C3 = 0;
            break;
        case 4:
            KB_C4 = 0;
            break;
        case 5:
            KB_C5 = 0;
            break;
        case 6:
            KB_C6 = 0;
            break;
        case 7:
            KB_C7 = 0;
            break;
        case 8:
            KB_C8 = 0;
            break;
        case 9:
            KB_C9 = 0;
            break;
        case 10:
            KB_C10 = 0;
            break;
        case 11:
            KB_C11 = 0;
            break;
        case 12:
            KB_C12 = 0;
            break;
        case 13:
            KB_C13 = 0;
            break;
        case 14:
            KB_C14 = 0;
            break;
        case 15:
            KB_C15 = 0;
            break;
    }
}

void user_matrix_col_deselect(uint8_t col) // active-low: drive HIGH (idle)
{
    switch (col) {
        case 0:
            KB_C0 = 1;
            break;
        case 1:
            KB_C1 = 1;
            break;
        case 2:
            KB_C2 = 1;
            break;
        case 3:
            KB_C3 = 1;
            break;
        case 4:
            KB_C4 = 1;
            break;
        case 5:
            KB_C5 = 1;
            break;
        case 6:
            KB_C6 = 1;
            break;
        case 7:
            KB_C7 = 1;
            break;
        case 8:
            KB_C8 = 1;
            break;
        case 9:
            KB_C9 = 1;
            break;
        case 10:
            KB_C10 = 1;
            break;
        case 11:
            KB_C11 = 1;
            break;
        case 12:
            KB_C12 = 1;
            break;
        case 13:
            KB_C13 = 1;
            break;
        case 14:
            KB_C14 = 1;
            break;
        case 15:
            KB_C15 = 1;
            break;
    }
}

// Bits 0-4 = rows R0..R4 (pressed = 0); bits 5-7 forced to 1 so they read as
// "not pressed" after matrix.c inverts the sample. Same wiring as the Air60.
uint8_t user_matrix_read_rows(void)
{
    return (uint8_t)(((P7 >> 1) & 0x07) | (P5 & 0x18) | 0xE0);
}
