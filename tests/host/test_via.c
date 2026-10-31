// Host unit tests for src/smk/via.c and vial.c: byte-exact requests and replies as the Vial GUI
// sends them (formats taken from vial-qmk's via.c / vial.c), against a mock keymap, matrix and clock.

#include "via.h"
#include "vial.h"
#include "kbdef.h"
#include <stdio.h>
#include <string.h>

// ---- the mocks ----------------------------------------------------------------------------------

static uint16_t keys[KEYMAP_DYNAMIC_LAYERS][MATRIX_ROWS][MATRIX_COLS];
static bool     reset_called;

uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    return keys[layer][row][col];
}

bool keymap_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    if (layer >= KEYMAP_DYNAMIC_LAYERS || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return false;
    }
    keys[layer][row][col] = keycode;
    return true;
}

void keymap_reset(void)
{
    reset_called = true;
}

uint8_t keymap_layer_count(void)
{
    return KEYMAP_DYNAMIC_LAYERS;
}

// encoders: [layer][encoder][ccw, cw]
static uint16_t encoder_keys[KEYMAP_DYNAMIC_LAYERS][ENCODER_COUNT][2];

uint16_t keymap_encoder_get(uint8_t layer, uint8_t encoder, bool clockwise)
{
    return encoder_keys[layer][encoder][clockwise ? 1 : 0];
}

bool keymap_encoder_set(uint8_t layer, uint8_t encoder, bool clockwise, uint16_t keycode)
{
    if (layer >= KEYMAP_DYNAMIC_LAYERS || encoder >= ENCODER_COUNT) {
        return false;
    }
    encoder_keys[layer][encoder][clockwise ? 1 : 0] = keycode;
    return true;
}

static bool    down[MATRIX_ROWS][MATRIX_COLS];
static uint32_t now;

bool matrix_is_on(uint8_t row, uint8_t col)
{
    return row < MATRIX_ROWS && col < MATRIX_COLS && down[row][col];
}

uint32_t timer_read32(void)
{
    return now;
}

uint16_t timer_read(void)
{
    return (uint16_t)now;
}

uint16_t timer_elapsed(uint16_t since)
{
    return (uint16_t)((uint16_t)now - since);
}

// the board data vial.c serves: an 80-byte "definition" so that 3 pages exist (32 + 32 + 16)
static uint8_t def_bytes[80];
const uint8_t vial_definition[80] = { 0 };
const uint32_t vial_definition_size = 80;
const uint8_t  vial_keyboard_uid[8] = { 0x46, 0xEC, 0xD0, 0x56, 0x5D, 0xF8, 0x4F, 0xA8 };
const uint8_t  vial_unlock_keys[][2] = { { 1, 0 }, { 0, 2 } };
const uint8_t  vial_unlock_key_count = 2;

// ---- helpers ------------------------------------------------------------------------------------

static int failures;
static int checks;

#define CHECK(cond)                                           \
    do {                                                      \
        checks++;                                             \
        if (!(cond)) {                                        \
            failures++;                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);    \
        }                                                     \
    } while (0)

static uint8_t msg[32];

static void send(const uint8_t *request, size_t n)
{
    memset(msg, 0, sizeof msg);
    memcpy(msg, request, n);
    via_receive(msg, 32);
}

#define SEND(...)                                         \
    do {                                                  \
        const uint8_t req[] = { __VA_ARGS__ };            \
        send(req, sizeof req);                            \
    } while (0)

static void unlock_for_real(void)
{
    SEND(0xFE, vial_unlock_start);
    down[1][0] = true;
    down[0][2] = true;
    for (int i = 0; i < 60 && !vial_is_unlocked(); i++) {
        now += 101;
        SEND(0xFE, vial_unlock_poll);
    }
    down[1][0] = false;
    down[0][2] = false;
}

static void lock(void)
{
    SEND(0xFE, vial_lock);
}

// ---- tests --------------------------------------------------------------------------------------

static void test_protocol_version(void)
{
    SEND(0x01);
    CHECK(msg[0] == 0x01 && msg[1] == 0x00 && msg[2] == 0x09);
}

static void test_get_set_keycode(void)
{
    SEND(0x05, 3, 1, 2, 0x12, 0x34); // set layer 3 row 1 col 2
    CHECK(keys[3][1][2] == 0x1234);

    SEND(0x04, 3, 1, 2);
    CHECK(msg[0] == 0x04 && msg[1] == 3 && msg[2] == 1 && msg[3] == 2); // request echoed
    CHECK(msg[4] == 0x12 && msg[5] == 0x34);                            // keycode, big-endian

    SEND(0x04, 9, 0, 0); // a layer that does not exist
    CHECK(msg[4] == 0 && msg[5] == 0);

    SEND(0x05, 9, 0, 0, 0xFF, 0xFF); // ...and a write to it changes nothing
    SEND(0x05, 0, 5, 0, 0xFF, 0xFF);
    SEND(0x05, 0, 0, 7, 0xFF, 0xFF);
    CHECK(keys[0][0][0] == 0);
}

static void test_layer_count_and_reset(void)
{
    SEND(0x11);
    CHECK(msg[1] == KEYMAP_DYNAMIC_LAYERS);

    reset_called = false;
    SEND(0x06);
    CHECK(reset_called);
}

static void test_buffer(void)
{
    memset(keys, 0, sizeof keys);
    keys[0][0][0] = 0x0102;
    keys[0][0][1] = 0x0304;
    keys[0][0][2] = 0x0506;
    keys[0][1][0] = 0x0708; // the buffer is layer, row, col; two bytes each
    keys[1][0][0] = 0x090A; // first cell of layer 1: byte offset 2*2*3 = 12

    SEND(0x12, 0x00, 0x00, 28);
    const uint8_t want[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    CHECK(memcmp(&msg[4], want, sizeof want) == 0);
    CHECK(msg[0] == 0x12 && msg[1] == 0 && msg[2] == 0 && msg[3] == 28);
    CHECK(msg[4 + 12] == 0x09 && msg[4 + 13] == 0x0A);

    // past the end of the keymap the reply is zero
    const uint16_t total = KEYMAP_DYNAMIC_LAYERS * MATRIX_ROWS * MATRIX_COLS * 2;
    SEND(0x12, (uint8_t)((total - 2) >> 8), (uint8_t)((total - 2) & 0xFF), 4);
    CHECK(msg[6] == 0 && msg[7] == 0);

    // a request for more than 28 bytes is ignored: the data bytes stay as sent
    SEND(0x12, 0, 0, 29, 0xAA);
    CHECK(msg[4] == 0xAA);

    // writes: whole cells, and an odd offset (low byte) on its own
    SEND(0x13, 0x00, 0x10, 4, 0xDE, 0xAD, 0xBE, 0xEF); // offset 16: layer 1, row 0, col 2 and col 3?  (2 rows x 3 cols = 6 cells/layer)
    CHECK(keys[1][0][2] == 0xDEAD);
    CHECK(keys[1][1][0] == 0xBEEF);
    SEND(0x13, 0x00, 0x11, 1, 0x55); // low byte of the cell at offset 16
    CHECK(keys[1][0][2] == 0xDE55);
    SEND(0x13, 0x00, 0x00, 29, 0x11); // too long
    CHECK(keys[0][0][0] == 0x0102);
}

static void test_macros_are_empty(void)
{
    SEND(0x0C, 0xFF);
    CHECK(msg[1] == 0);
    SEND(0x0D, 0xFF, 0xFF);
    CHECK(msg[1] == 0 && msg[2] == 0);
}

static void test_keyboard_values(void)
{
    now = 0x01020304;
    SEND(0x02, 0x01);
    CHECK(msg[2] == 0x01 && msg[3] == 0x02 && msg[4] == 0x03 && msg[5] == 0x04); // uptime, big-endian

    SEND(0x03, 0x02, 0xA1, 0xB2, 0xC3, 0xD4); // layout options
    SEND(0x02, 0x02);
    CHECK(msg[2] == 0xA1 && msg[3] == 0xB2 && msg[4] == 0xC3 && msg[5] == 0xD4);

    SEND(0x02, 0x7F); // a value this keyboard does not have
    CHECK(msg[0] == 0xFF);
}

static void test_matrix_state_needs_unlock(void)
{
    lock();
    down[1][2] = true;
    SEND(0x02, 0x03);
    CHECK(msg[0] == 0x02 && msg[1] == 0x03 && msg[2] == 0 && msg[3] == 0); // untouched

    unlock_for_real();
    CHECK(vial_is_unlocked());
    down[1][2] = true;
    SEND(0x02, 0x03);
    // one byte per row (3 columns), data from msg[2]; row 0 nothing, row 1 col 2 -> 0x04
    CHECK(msg[2] == 0x00 && msg[3] == 0x04);
    down[1][2] = false;
    lock();
}

static void test_bootloader_jump_needs_unlock(void)
{
    lock();
    SEND(0x0B);
    CHECK(!via_bootloader_requested());
    unlock_for_real();
    SEND(0x0B);
    CHECK(via_bootloader_requested());
    lock();
}

static void test_unhandled(void)
{
    SEND(0x07, 0x01, 0x02); // custom value: nothing here yet
    CHECK(msg[0] == 0xFF);
    SEND(0x0A);             // eeprom reset: not offered
    CHECK(msg[0] == 0xFF);
    SEND(0x42);
    CHECK(msg[0] == 0xFF);
}

static void test_vial_identity(void)
{
    SEND(0xFE, vial_get_keyboard_id);
    CHECK(msg[0] == 6 && msg[1] == 0 && msg[2] == 0 && msg[3] == 0); // protocol 6, little-endian
    CHECK(memcmp(&msg[4], vial_keyboard_uid, 8) == 0);
    CHECK(msg[12] == 0 && msg[31] == 0);

    SEND(0xFE, vial_get_size);
    CHECK(msg[0] == 80 && msg[1] == 0 && msg[2] == 0 && msg[3] == 0);
}

static void test_vial_definition_pages(void)
{
    (void)def_bytes;
    SEND(0xFE, vial_get_def, 0, 0);
    CHECK(msg[0] == 0 && msg[31] == 0); // the mock definition is all zero, the page is a full 32 bytes
    // the third page is the last 16 bytes; past it the request is returned as it came
    SEND(0xFE, vial_get_def, 3, 0);
    CHECK(msg[0] == 0xFE && msg[1] == vial_get_def && msg[2] == 3);
}

static void test_unlock_flow(void)
{
    lock();
    now = 1000;

    SEND(0xFE, vial_get_unlock_status);
    CHECK(msg[0] == 0 && msg[1] == 0);
    CHECK(msg[2] == 1 && msg[3] == 0 && msg[4] == 0 && msg[5] == 2); // (row, col) pairs, then 0xFF
    CHECK(msg[6] == 0xFF && msg[31] == 0xFF);

    SEND(0xFE, vial_unlock_start);

    // keys not held: no progress, counter stays at 50
    now += 500;
    SEND(0xFE, vial_unlock_poll);
    CHECK(msg[0] == 0 && msg[1] == 1 && msg[2] == 50);

    // while it runs every other command is answered with the request unchanged
    SEND(0x04, 0, 0, 0);
    CHECK(msg[0] == 0x04 && msg[4] == 0 && msg[5] == 0);
    SEND(0x11);
    CHECK(msg[1] == 0); // not the layer count
    SEND(0xFE, vial_get_keyboard_id);
    CHECK(msg[0] == 6); // the unlock questions are still answered

    // held and a step has passed since the last count: one step done ...
    down[1][0] = true;
    down[0][2] = true;
    SEND(0xFE, vial_unlock_poll);
    CHECK(msg[1] == 1 && msg[2] == 49);

    // ... and a second poll straight after is too soon: vial-qmk resets the count (the GUI polls every ~100 ms)
    SEND(0xFE, vial_unlock_poll);
    CHECK(msg[2] == 50);

    // held and polled every 101 ms: 50 steps
    for (int step = 1; step <= 49; step++) {
        now += 101;
        SEND(0xFE, vial_unlock_poll);
        CHECK(msg[0] == 0 && msg[1] == 1 && msg[2] == 50 - step);
    }

    // a key let go resets it
    down[0][2] = false;
    now += 101;
    SEND(0xFE, vial_unlock_poll);
    CHECK(msg[2] == 50 && !vial_is_unlocked());

    down[0][2] = true;
    for (int step = 0; step < 50; step++) {
        now += 101;
        SEND(0xFE, vial_unlock_poll);
    }
    CHECK(msg[0] == 1 && msg[1] == 0 && msg[2] == 0);
    CHECK(vial_is_unlocked());

    SEND(0x11); // commands work again
    CHECK(msg[1] == KEYMAP_DYNAMIC_LAYERS);

    down[1][0] = false;
    down[0][2] = false;
    lock();
    CHECK(!vial_is_unlocked());
}

static void test_vial_extras(void)
{
    SEND(0xFE, vial_qmk_settings_query, 0, 0);
    CHECK(msg[0] == 0xFF && msg[31] == 0xFF); // no QMK settings

    SEND(0xFE, vial_dynamic_entry_op, 0x00);
    for (int i = 0; i < 32; i++) {
        CHECK(msg[i] == 0); // no tap dance, combos, key overrides, alt repeat; no optional features
    }
}

static void test_encoders(void)
{
    // set counter-clockwise then clockwise of encoder 0 on layer 2
    SEND(0xFE, vial_set_encoder, 2, 0, 0, 0x00, 0xEA);
    SEND(0xFE, vial_set_encoder, 2, 0, 1, 0x00, 0xE9);
    CHECK(encoder_keys[2][0][0] == 0x00EA && encoder_keys[2][0][1] == 0x00E9);

    // the reply to a read carries both: counter-clockwise first, big-endian
    SEND(0xFE, vial_get_encoder, 2, 0);
    CHECK(msg[0] == 0x00 && msg[1] == 0xEA && msg[2] == 0x00 && msg[3] == 0xE9);

    // a layer or an encoder that does not exist answers zeros, and a write to it changes nothing
    SEND(0xFE, vial_get_encoder, 9, 0);
    CHECK(msg[0] == 0 && msg[1] == 0 && msg[2] == 0 && msg[3] == 0);
    SEND(0xFE, vial_get_encoder, 0, 5);
    CHECK(msg[0] == 0 && msg[1] == 0 && msg[2] == 0 && msg[3] == 0);
    SEND(0xFE, vial_set_encoder, 9, 0, 1, 0xFF, 0xFF);
    SEND(0xFE, vial_set_encoder, 0, 5, 1, 0xFF, 0xFF);
    CHECK(encoder_keys[0][0][1] == 0);
}

static void test_wrong_length_is_ignored(void)
{
    memset(msg, 0x55, sizeof msg);
    via_receive(msg, 31);
    CHECK(msg[0] == 0x55);
}

int main(void)
{
    test_protocol_version();
    test_get_set_keycode();
    test_layer_count_and_reset();
    test_buffer();
    test_macros_are_empty();
    test_keyboard_values();
    test_matrix_state_needs_unlock();
    test_bootloader_jump_needs_unlock();
    test_unhandled();
    test_vial_identity();
    test_vial_definition_pages();
    test_unlock_flow();
    test_vial_extras();
    test_encoders();
    test_wrong_length_is_ignored();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
