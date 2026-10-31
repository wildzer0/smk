#include "keymap.h"
#include "layer.h"
#include "layout.h"

// The only place that reads `keymaps` directly: a runtime-editable keymap replaces these bodies.
uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    return keymaps[layer][row][col];
}

uint8_t keymap_layer_count(void)
{
    return keymap_layers;
}
