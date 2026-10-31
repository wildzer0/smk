#include "hud.h"

#ifdef HUD_ENABLE

#include "keymap.h"
#include "layer.h"
#include "matrix.h"
#include "report.h"
#include "keyboard.h"
#include <string.h>

#define HUD_PROTOCOL_V1 1
#define HUD_PROTOCOL_V2 2
#define HUD_KEYMAP_MAX  12 // keycodes per GET_KEYMAP reply

#define WIDE_MATRIX      (MATRIX_ROWS > 8 || MATRIX_COLS > 8)
#define BYTES_PER_ROW    ((MATRIX_COLS + 7) / 8)
#define MATRIX_BYTES     (MATRIX_ROWS * BYTES_PER_ROW)
#define STATE_ROWS       (MATRIX_ROWS > 8 ? 8 : MATRIX_ROWS) // GET_STATE has room for eight rows

_Static_assert(MATRIX_BYTES <= 28, "GET_MATRIX must fit one packet");

enum { HUD_GET_INFO = 0x01, HUD_GET_STATE = 0x02, HUD_GET_KEYMAP = 0x03, HUD_GET_MATRIX = 0x04 };
enum { HUD_OK = 0, HUD_ERR_CMD = 1, HUD_ERR_ARG = 2 };

// capability bits (GET_INFO)
enum { HUD_CAP_HUD_TOG = 1 << 3 };

// flag bits (GET_STATE)
enum {
    HUD_FLAG_CAPS_LOCK = 1 << 0,
    HUD_FLAG_NUM_LOCK  = 1 << 4,
};

// HID LED report bits
#define LED_NUM_LOCK  (1u << 0)
#define LED_CAPS_LOCK (1u << 1)

static uint8_t toggle_count; // +1 on every HUD_TOG press

void hud_toggle_pressed(void)
{
    toggle_count++;
}

uint16_t hud_toggle_keycode(void)
{
#ifdef HUD_TOGGLE_KEYCODE
    return HUD_TOGGLE_KEYCODE;
#else
    return 0;
#endif
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, (uint16_t)(v & 0xFFFFu));
    put16(p + 2, (uint16_t)(v >> 16));
}

// Pressed keys of one row: bit c = the key in column c, packed little-endian across `first_byte`..
static uint8_t row_byte(uint8_t row, uint8_t byte_index)
{
    uint8_t value = 0;

    for (uint8_t bit = 0; bit < 8; bit++) {
        const uint8_t col = (uint8_t)(byte_index * 8 + bit);

        if (col < MATRIX_COLS && matrix_is_on(row, col)) {
            value |= (uint8_t)(1u << bit);
        }
    }

    return value;
}

static void get_info(uint8_t *data)
{
    uint16_t caps = 0;

    const uint16_t toggle = hud_toggle_keycode();
    if (toggle) {
        caps |= HUD_CAP_HUD_TOG;
    }

    data[4] = WIDE_MATRIX ? HUD_PROTOCOL_V2 : HUD_PROTOCOL_V1;
    data[5] = MATRIX_ROWS;
    data[6] = MATRIX_COLS;
    data[7] = keymap_layer_count();
    put16(&data[8], caps);
    put16(&data[10], toggle);
    data[12] = HUD_KEYMAP_MAX;
    data[13] = keymap_version();
    data[14] = BYTES_PER_ROW; // protocol 2
}

static void get_state(uint8_t *data)
{
    put32(&data[4], (uint32_t)layer_get_state());
    data[8] = (uint8_t)(1u << layer_get_default());
    data[9] = 0; // no layer lock

    // the first eight columns of up to eight rows, as protocol 1 has it (data[10..17])
    for (uint8_t row = 0; row < STATE_ROWS; row++) {
        data[10 + row] = row_byte(row, 0);
    }

    data[18] = get_mods();
    data[19] = get_weak_mods();
    data[20] = layer_oneshot_mods();
    data[21] = 0; // no locked one-shot mods
    data[22] = layer_oneshot_layer();

    uint8_t flags = 0;
    if (keyboard_state.led_state & LED_CAPS_LOCK) {
        flags |= HUD_FLAG_CAPS_LOCK;
    }
    if (keyboard_state.led_state & LED_NUM_LOCK) {
        flags |= HUD_FLAG_NUM_LOCK;
    }
    data[23] = flags;

    // 24..26: OS detection, OS mode and active OS: none of those exist here
    data[27] = toggle_count;
    data[28] = keymap_version();
}

static void get_matrix(uint8_t *data)
{
    uint8_t at = 4;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t byte = 0; byte < BYTES_PER_ROW; byte++) {
            data[at++] = row_byte(row, byte);
        }
    }
}

// The EFFECTIVE keycodes of `count` keys from position `offset` (row * cols + col) on a layer.
static void get_keymap(uint8_t *data, const uint8_t *args)
{
    const uint8_t  layer  = args[0];
    const uint16_t offset = (uint16_t)(args[1] | ((uint16_t)args[2] << 8));
    const uint8_t  count  = args[3];

    memcpy(&data[4], args, 4); // the request is echoed

    if (layer >= keymap_layer_count() || count == 0 || count > HUD_KEYMAP_MAX || offset + count > MATRIX_ROWS * MATRIX_COLS) {
        data[2] = HUD_ERR_ARG;
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        const uint16_t position = (uint16_t)(offset + i);
        put16(&data[8 + 2 * i], keymap_get(layer, (uint8_t)(position / MATRIX_COLS), (uint8_t)(position % MATRIX_COLS)));
    }
}

bool hud_receive(uint8_t *data, uint8_t length)
{
    if (data[0] != HUD_CMD || length < 32) {
        return false;
    }

    // data[1] (the command) and data[3] (the app's sequence number) stay as they are
    uint8_t args[4];
    memcpy(args, &data[4], sizeof(args));
    memset(&data[4], 0, (size_t)(length - 4));
    data[2] = HUD_OK;

    switch (data[1]) {
        case HUD_GET_INFO:
            get_info(data);
            break;
        case HUD_GET_STATE:
            get_state(data);
            break;
        case HUD_GET_KEYMAP:
            get_keymap(data, args);
            break;
        case HUD_GET_MATRIX:
            get_matrix(data);
            break;
        default:
            data[2] = HUD_ERR_CMD;
            break;
    }

    return true;
}

#endif // HUD_ENABLE
