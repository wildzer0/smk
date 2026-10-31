#include "via.h"
#include "vial.h"
#include "keymap.h"
#include "layer.h"
#include "matrix.h"
#include "timer.h"
#include "kbdef.h"
#ifdef HUD_ENABLE
#    include "hud.h"
#endif
#ifdef KEYMAP_MACRO_BYTES
#    include "keymap_store.h"
#endif

#define VIA_PROTOCOL_VERSION 0x0009u

enum {
    id_get_protocol_version                 = 0x01,
    id_get_keyboard_value                   = 0x02,
    id_set_keyboard_value                   = 0x03,
    id_dynamic_keymap_get_keycode           = 0x04,
    id_dynamic_keymap_set_keycode           = 0x05,
    id_dynamic_keymap_reset                 = 0x06,
    id_bootloader_jump                      = 0x0B,
    id_dynamic_keymap_macro_get_count       = 0x0C,
    id_dynamic_keymap_macro_get_buffer_size = 0x0D,
    id_dynamic_keymap_macro_get_buffer      = 0x0E,
    id_dynamic_keymap_macro_set_buffer      = 0x0F,
    id_dynamic_keymap_macro_reset           = 0x10,
    id_dynamic_keymap_get_layer_count       = 0x11,
    id_dynamic_keymap_get_buffer            = 0x12,
    id_dynamic_keymap_set_buffer            = 0x13,
    id_vial_prefix                          = 0xFE,
    id_unhandled                            = 0xFF,
};

enum {
    id_uptime              = 0x01,
    id_layout_options      = 0x02,
    id_switch_matrix_state = 0x03,
};

#define MAX_CHUNK 28u // a request carries at most 28 bytes of buffer data

// the layout-option bits the GUI stores for keyboards with layout variants (kept in RAM only for now)
static uint32_t layout_options;

static bool bootloader_requested;

bool via_bootloader_requested(void)
{
    return bootloader_requested;
}

#define BUFFER_BYTES ((uint16_t)(KEYMAP_DYNAMIC_LAYERS * MATRIX_ROWS * MATRIX_COLS * 2u))

// The keymap as the GUI sees it: layer by layer, row by row, each keycode big-endian.
static uint8_t buffer_byte_get(uint16_t offset)
{
    if (offset >= BUFFER_BYTES) {
        return 0;
    }

    const uint16_t word  = offset / 2u;
    const uint16_t cells = (uint16_t)(MATRIX_ROWS * MATRIX_COLS);
    const uint16_t within = word % cells;
    const uint16_t keycode = keymap_get((uint8_t)(word / cells), (uint8_t)(within / MATRIX_COLS), (uint8_t)(within % MATRIX_COLS));

    return (offset & 1u) ? (uint8_t)(keycode & 0xFFu) : (uint8_t)(keycode >> 8);
}

static void buffer_byte_set(uint16_t offset, uint8_t value)
{
    if (offset >= BUFFER_BYTES) {
        return;
    }

    const uint16_t word    = offset / 2u;
    const uint16_t cells   = (uint16_t)(MATRIX_ROWS * MATRIX_COLS);
    const uint16_t within  = word % cells;
    const uint8_t  layer   = (uint8_t)(word / cells);
    const uint8_t  row     = (uint8_t)(within / MATRIX_COLS);
    const uint8_t  col     = (uint8_t)(within % MATRIX_COLS);
    uint16_t       keycode = keymap_get(layer, row, col);

    if (offset & 1u) {
        keycode = (uint16_t)((keycode & 0xFF00u) | value);
    } else {
        keycode = (uint16_t)((keycode & 0x00FFu) | ((uint16_t)value << 8));
    }

    keymap_set(layer, row, col, keycode);
}

static void put_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static void get_keyboard_value(uint8_t *data)
{
    uint8_t *args = &data[1];

    switch (args[0]) {
        case id_uptime:
            put_u32(&args[1], timer_read32());
            break;

        case id_layout_options:
            put_u32(&args[1], layout_options);
            break;

        case id_switch_matrix_state: {
            // the GUI's matrix tester: it shows every key press, so only an unlocked keyboard answers
            if (!vial_is_unlocked()) {
                break;
            }

            const uint8_t per_row = (MATRIX_COLS > 24) ? 4u : (MATRIX_COLS > 16) ? 3u : (MATRIX_COLS > 8) ? 2u : 1u;
            _Static_assert(((MATRIX_COLS > 24) ? 4 : (MATRIX_COLS > 16) ? 3 : (MATRIX_COLS > 8) ? 2 : 1) * MATRIX_ROWS <= 28, "the matrix state does not fit in one report");

            uint8_t at = 1;
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                uint32_t value = 0;
                for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                    if (matrix_is_on(row, col)) {
                        value |= (uint32_t)1u << col;
                    }
                }
                for (uint8_t b = per_row; b > 0; b--) {
                    args[at++] = (uint8_t)(value >> (8u * (b - 1u)));
                }
            }
            break;
        }

        default:
            data[0] = id_unhandled;
            break;
    }
}

static void set_keyboard_value(uint8_t *data)
{
    const uint8_t *args = &data[1];

    if (args[0] == id_layout_options) {
        layout_options = ((uint32_t)args[1] << 24) | ((uint32_t)args[2] << 16) | ((uint32_t)args[3] << 8) | args[4];
    } else {
        data[0] = id_unhandled;
    }
}

void via_receive(uint8_t *data, uint8_t length)
{
    if (length != VIA_RAW_SIZE) {
        return;
    }

    uint8_t *args = &data[1];

    // while an unlock runs the GUI may only ask the unlock questions; everything else is echoed back untouched
    if (vial_unlock_in_progress()) {
        const uint8_t cmd = data[1];
        if (data[0] != id_vial_prefix || (cmd != vial_get_keyboard_id && cmd != vial_get_size && cmd != vial_get_def && cmd != vial_get_unlock_status && cmd != vial_unlock_start && cmd != vial_unlock_poll)) {
            return;
        }
    }

    switch (data[0]) {
        case id_get_protocol_version:
            args[0] = (uint8_t)(VIA_PROTOCOL_VERSION >> 8);
            args[1] = (uint8_t)(VIA_PROTOCOL_VERSION & 0xFFu);
            break;

        case id_get_keyboard_value:
            get_keyboard_value(data);
            break;

        case id_set_keyboard_value:
            set_keyboard_value(data);
            break;

        case id_dynamic_keymap_get_keycode: {
            const uint16_t keycode = (args[0] < keymap_layer_count() && args[1] < MATRIX_ROWS && args[2] < MATRIX_COLS) ? keymap_get(args[0], args[1], args[2]) : 0u;
            args[3]                = (uint8_t)(keycode >> 8);
            args[4]                = (uint8_t)(keycode & 0xFFu);
            break;
        }

        case id_dynamic_keymap_set_keycode:
            keymap_set(args[0], args[1], args[2], (uint16_t)(((uint16_t)args[3] << 8) | args[4]));
            break;

        case id_dynamic_keymap_reset:
            keymap_reset();
            break;

        case id_dynamic_keymap_get_layer_count:
            args[0] = keymap_layer_count();
            break;

        case id_dynamic_keymap_get_buffer: {
            const uint16_t offset = (uint16_t)(((uint16_t)args[0] << 8) | args[1]);
            const uint8_t  size   = args[2];

            if (size <= MAX_CHUNK) {
                for (uint8_t i = 0; i < size; i++) {
                    args[3 + i] = buffer_byte_get((uint16_t)(offset + i));
                }
            }
            break;
        }

        case id_dynamic_keymap_set_buffer: {
            const uint16_t offset = (uint16_t)(((uint16_t)args[0] << 8) | args[1]);
            const uint8_t  size   = args[2];

            if (size <= MAX_CHUNK) {
                for (uint8_t i = 0; i < size; i++) {
                    buffer_byte_set((uint16_t)(offset + i), args[3 + i]);
                }
            }
            break;
        }

#ifdef KEYMAP_MACRO_BYTES
        case id_dynamic_keymap_macro_get_count:
            args[0] = KEYMAP_MACRO_COUNT;
            break;

        case id_dynamic_keymap_macro_get_buffer_size:
            args[0] = (uint8_t)(KEYMAP_MACRO_BYTES >> 8);
            args[1] = (uint8_t)(KEYMAP_MACRO_BYTES & 0xFFu);
            break;

        case id_dynamic_keymap_macro_get_buffer: {
            const uint16_t offset = (uint16_t)(((uint16_t)args[0] << 8) | args[1]);
            const uint8_t  size   = args[2];

            if (size <= MAX_CHUNK) {
                keymap_store_macro_read(offset, size, &args[3]);
            }
            break;
        }

        case id_dynamic_keymap_macro_set_buffer: {
            // until the keyboard is unlocked the macros cannot be changed: the request is echoed back untouched
            const uint16_t offset = (uint16_t)(((uint16_t)args[0] << 8) | args[1]);
            const uint8_t  size   = args[2];

            if (vial_is_unlocked() && size <= MAX_CHUNK) {
                keymap_store_macro_write(offset, size, &args[3]);
            }
            break;
        }

        case id_dynamic_keymap_macro_reset:
            if (vial_is_unlocked()) {
                keymap_store_macro_clear();
            }
            break;
#else
        // no macro buffer: the GUI then offers no macro editor
        case id_dynamic_keymap_macro_get_count:
            args[0] = 0;
            break;

        case id_dynamic_keymap_macro_get_buffer_size:
            args[0] = 0;
            args[1] = 0;
            break;

        case id_dynamic_keymap_macro_get_buffer:
        case id_dynamic_keymap_macro_set_buffer:
        case id_dynamic_keymap_macro_reset:
            break;
#endif

        case id_bootloader_jump:
            // like the other dangerous commands it needs the unlock; the reply goes out first, then the jump
            if (vial_is_unlocked()) {
                bootloader_requested = true;
            }
            break;

        case id_vial_prefix:
            vial_receive(data, length);
            break;

        default:
#ifdef HUD_ENABLE
            if (hud_receive(data, length)) {
                break; // the keyboard HUD's own command
            }
#endif
            data[0] = id_unhandled;
            break;
    }
}
