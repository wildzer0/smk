#include "keymap.h"
#include "layer.h"
#include "layout.h"

#ifdef FLASH_KEYMAP_ADDR

#include "keymap_store.h"
#include "matrix.h"
#include "tick.h"

// Edits are written once nothing has changed for this many matrix sweeps (a second or two)
// and no key is held, so a flash write never lands in the middle of typing.
#define FLUSH_AFTER_SWEEPS 2000u

static uint16_t last_edit;

uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    return (layer < keymap_layers) ? keymaps[layer][row][col] : KC_TRANSPARENT;
}

uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    return keymap_store_get(layer, row, col);
}

uint8_t keymap_layer_count(void)
{
    return KEYMAP_DYNAMIC_LAYERS;
}

void keymap_init(void)
{
    keymap_store_init();
}

bool keymap_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    last_edit = tick_sweeps();
    return keymap_store_set(layer, row, col, keycode);
}

void keymap_reset(void)
{
    last_edit = tick_sweeps();
    keymap_store_reset();
}

void keymap_task(void)
{
    if (keymap_store_dirty() && (uint16_t)(tick_sweeps() - last_edit) >= FLUSH_AFTER_SWEEPS && !matrix_any_pressed()) {
        keymap_store_flush();
    }
}

#else

// The only place that reads `keymaps` directly for a fixed keymap.
uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    return keymaps[layer][row][col];
}

uint8_t keymap_layer_count(void)
{
    return keymap_layers;
}

void keymap_init(void) {}

bool keymap_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    layer;
    row;
    col;
    keycode;
    return false;
}

void keymap_reset(void) {}

void keymap_task(void) {}

#endif
