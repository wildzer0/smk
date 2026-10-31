// Host unit tests for src/smk/hud.c: the keyboard HUD protocol. Built twice: on the 2x3 stand-in board
// (protocol 1) and with -DMATRIX_ROWS=5 -DMATRIX_COLS=16 (protocol 2), like the B68.

#include "hud.h"
#include "keyboard.h"
#include "keycodes.h"
#include <stdio.h>
#include <string.h>

// ---- the firmware around it, mocked ---------------------------------------------------------------
static uint16_t keys[3][MATRIX_ROWS][MATRIX_COLS];
static bool     pressed[MATRIX_ROWS][MATRIX_COLS];
static uint8_t  state_layers, default_layer, oneshot_layer = 0xFF, oneshot_mods, mods, weak_mods, version;

uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    return keys[layer][row][col];
}

uint8_t keymap_layer_count(void)
{
    return 3;
}

uint8_t keymap_version(void)
{
    return version;
}

uint8_t layer_get_state(void)
{
    return state_layers;
}

uint8_t layer_get_default(void)
{
    return default_layer;
}

uint8_t layer_oneshot_layer(void)
{
    return oneshot_layer;
}

uint8_t layer_oneshot_mods(void)
{
    return oneshot_mods;
}

bool matrix_is_on(uint8_t row, uint8_t col)
{
    return pressed[row][col];
}

uint8_t get_mods(void)
{
    return mods;
}

uint8_t get_weak_mods(void)
{
    return weak_mods;
}

volatile keyboard_state_t keyboard_state;

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

static void ask(uint8_t command, uint8_t sequence, const uint8_t *args, int n)
{
    memset(msg, 0, sizeof msg);
    msg[0] = 0xF0;
    msg[1] = command;
    msg[3] = sequence;
    for (int i = 0; i < n; i++) {
        msg[4 + i] = args[i];
    }
    CHECK(hud_receive(msg, 32));
}

#define WIDE (MATRIX_COLS > 8)

static void test_not_a_hud_packet(void)
{
    uint8_t other[32] = { 0x01 };
    CHECK(!hud_receive(other, 32));
    uint8_t shortpkt[16] = { 0xF0 };
    CHECK(!hud_receive(shortpkt, 16));
}

static void test_info(void)
{
    ask(0x01, 7, 0, 0);
    CHECK(msg[0] == 0xF0 && msg[1] == 0x01 && msg[2] == 0 && msg[3] == 7); // command, status ok, sequence echoed
    CHECK(msg[4] == (WIDE ? 2 : 1));
    CHECK(msg[5] == MATRIX_ROWS && msg[6] == MATRIX_COLS && msg[7] == 3);
    CHECK((msg[8] | (msg[9] << 8)) == (1 << 3)); // the HUD_TOG capability
    CHECK((msg[10] | (msg[11] << 8)) == 0x7E00);
    CHECK(msg[12] == 12);
    CHECK(msg[14] == (MATRIX_COLS + 7) / 8);
}

static void test_state(void)
{
    state_layers  = 0x05;
    default_layer = 1;
    oneshot_layer = 2;
    oneshot_mods  = 0x02;
    mods          = 0x01;
    weak_mods     = 0x04;
    version       = 9;
    keyboard_state.led_state = 0x03; // num lock + caps lock
    memset(pressed, 0, sizeof pressed);
    pressed[1][2] = true;
    pressed[0][0] = true;

    ask(0x02, 1, 0, 0);
    CHECK(msg[2] == 0 && msg[3] == 1);
    CHECK(msg[4] == 0x05 && msg[5] == 0 && msg[6] == 0 && msg[7] == 0); // layer mask
    CHECK(msg[8] == 0x02);                                                // default layer 1 as a mask
    CHECK(msg[10] == 0x01);                                               // row 0: column 0
    CHECK(msg[11] == 0x04);                                               // row 1: column 2
    CHECK(msg[18] == 0x01 && msg[19] == 0x04 && msg[20] == 0x02 && msg[21] == 0);
    CHECK(msg[22] == 2);
    CHECK(msg[23] == ((1 << 0) | (1 << 4))); // caps lock, num lock
    CHECK(msg[28] == 9);

    oneshot_layer = 0xFF; // none waiting
    ask(0x02, 2, 0, 0);
    CHECK(msg[22] == 0xFF);
}

#if MATRIX_COLS > 8
static void test_wide_matrix(void)
{
    memset(pressed, 0, sizeof pressed);
    pressed[0][1]  = true;  // low byte of row 0
    pressed[0][10] = true;  // high byte of row 0, bit 2
    pressed[1][15] = true;  // high byte of row 1, bit 7

    ask(0x04, 3, 0, 0);
    CHECK(msg[2] == 0 && msg[3] == 3);
    CHECK(msg[4] == 0x02 && msg[5] == 0x04); // row 0
    CHECK(msg[6] == 0x00 && msg[7] == 0x80); // row 1
    CHECK(msg[8] == 0 && msg[9] == 0);

    // GET_STATE keeps the first eight columns only
    ask(0x02, 4, 0, 0);
    CHECK(msg[10] == 0x02 && msg[11] == 0x00);
}
#endif

static void test_keymap(void)
{
    memset(keys, 0, sizeof keys);
    keys[1][0][1] = 0x1234;
    keys[1][1][0] = 0x5678;

    uint8_t args[4] = { 1, 1, 0, 2 }; // layer 1, from position 1, two keys
    ask(0x03, 5, args, 4);
    CHECK(msg[2] == 0);
    CHECK(msg[4] == 1 && msg[5] == 1 && msg[6] == 0 && msg[7] == 2); // the request is echoed
    CHECK(msg[8] == 0x34 && msg[9] == 0x12);                        // keycodes, little-endian
    CHECK(msg[10] == (keys[1][(1 + 1) / MATRIX_COLS][(1 + 1) % MATRIX_COLS] & 0xFF));

    uint8_t bad_layer[4] = { 3, 0, 0, 1 };
    ask(0x03, 6, bad_layer, 4);
    CHECK(msg[2] == 2);
    uint8_t bad_count[4] = { 0, 0, 0, 13 };
    ask(0x03, 6, bad_count, 4);
    CHECK(msg[2] == 2);
    uint8_t zero[4] = { 0, 0, 0, 0 };
    ask(0x03, 6, zero, 4);
    CHECK(msg[2] == 2);
    uint8_t past_end[4] = { 0, (uint8_t)(MATRIX_ROWS * MATRIX_COLS), 0, 1 };
    ask(0x03, 6, past_end, 4);
    CHECK(msg[2] == 2);
}

static void test_unknown_command_and_toggle(void)
{
    ask(0x7F, 8, 0, 0);
    CHECK(msg[2] == 1 && msg[3] == 8);

    ask(0x02, 9, 0, 0);
    const uint8_t before = msg[27];
    hud_toggle_pressed();
    ask(0x02, 9, 0, 0);
    CHECK((uint8_t)(msg[27] - before) == 1);
}

int main(void)
{
    test_not_a_hud_packet();
    test_info();
    test_state();
#if MATRIX_COLS > 8
    test_wide_matrix();
#endif
    test_keymap();
    test_unknown_command_and_toggle();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
