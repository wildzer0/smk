// Host unit tests for src/smk/encoder.c: the quadrature decoder fed with pin states, and the
// main-loop part that turns steps into key taps.

#include "encoder.h"
#include <stdio.h>

uint8_t test_encoder_state;

// the keycodes the layer lookup hands out, and the taps the encoder produced
static uint16_t taps[64];
static int      tap_count;

uint16_t layer_lookup_encoder(uint8_t encoder, bool clockwise)
{
    (void)encoder;
    return clockwise ? 0x1111 : 0x2222;
}

void matrix_tap_keycode(uint16_t keycode)
{
    if (tap_count < 64) {
        taps[tap_count++] = keycode;
    }
}

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

// Pin states are (A) | (B << 1). The probe on the real board saw A rise first, then B, then A fall,
// then B fall in one direction: 0 -> 1 -> 3 -> 2 -> 0, taken as clockwise.
static const uint8_t forward[4]  = { 1, 3, 2, 0 };
static const uint8_t backward[4] = { 2, 3, 1, 0 };

static void feed(uint8_t state)
{
    test_encoder_state = state;
    encoder_sample();
}

static void turn(const uint8_t *cycle, int cycles)
{
    for (int c = 0; c < cycles; c++) {
        for (int i = 0; i < 4; i++) {
            feed(cycle[i]);
        }
    }
}

static void reset(void)
{
    test_encoder_state = 0;
    tap_count          = 0;
    feed(0);
    encoder_task(); // flush what earlier tests left
    tap_count = 0;
}

static void test_one_detent_each_way(void)
{
    reset();
    turn(forward, 1);
    encoder_task();
    CHECK(tap_count == 1 && taps[0] == 0x1111);

    tap_count = 0;
    turn(backward, 1);
    encoder_task();
    CHECK(tap_count == 1 && taps[0] == 0x2222);
}

static void test_partial_detent_counts_nothing(void)
{
    reset();
    feed(1);
    feed(3);
    feed(2); // three of four changes
    encoder_task();
    CHECK(tap_count == 0);
    feed(0); // the fourth completes it
    encoder_task();
    CHECK(tap_count == 1 && taps[0] == 0x1111);
}

static void test_bounce_cancels(void)
{
    reset();
    feed(1);
    feed(0); // forward and back
    feed(1);
    feed(0);
    feed(1);
    feed(0);
    encoder_task();
    CHECK(tap_count == 0);
    turn(forward, 1); // and a real turn afterwards still counts as exactly one
    encoder_task();
    CHECK(tap_count == 1);
}

static void test_missed_sample_is_ignored(void)
{
    reset();
    feed(3); // 0 -> 3 is two changes at once: nothing counted
    encoder_task();
    CHECK(tap_count == 0);
}

static void test_many_steps_are_not_lost(void)
{
    reset();
    turn(forward, 20);
    encoder_task(); // 8 taps per pass
    CHECK(tap_count == 8);
    encoder_task();
    CHECK(tap_count == 16);
    encoder_task();
    CHECK(tap_count == 20);
    encoder_task();
    CHECK(tap_count == 20);
}

static void test_direction_change(void)
{
    reset();
    turn(forward, 2);
    encoder_task();
    turn(backward, 1);
    encoder_task();
    CHECK(tap_count == 3);
    CHECK(taps[0] == 0x1111 && taps[1] == 0x1111 && taps[2] == 0x2222);

    // steps that cancel before the main loop looks are never sent
    tap_count = 0;
    turn(forward, 1);
    turn(backward, 1);
    encoder_task();
    CHECK(tap_count == 0);
}

int main(void)
{
    test_one_detent_each_way();
    test_partial_detent_counts_nothing();
    test_bounce_cancels();
    test_missed_sample_is_ignored();
    test_many_steps_are_not_lost();
    test_direction_change();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
