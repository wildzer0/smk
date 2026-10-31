#include "encoder.h"
#include "kbdef.h"

#ifdef ENCODER_COUNT

#include "layer.h"
#include "matrix.h"

#ifndef ENCODER_RESOLUTION
#    define ENCODER_RESOLUTION 4
#endif

// Change in position for a move from state `old` to state `new` (index old * 4 + new). The pins
// step through the Gray code 0 -> 1 -> 3 -> 2 -> 0 one way and the reverse the other way; a jump of
// two states is a missed sample and counts for nothing, and a contact that bounces forward and
// back cancels out.
static const __code int8_t delta[16] = {
    0,  1, -1, 0, // from 0
    -1, 0, 0,  1, // from 1
    1,  0, 0,  -1, // from 2
    0,  -1, 1, 0, // from 3
};

static uint8_t       last[ENCODER_COUNT];
static int8_t        position[ENCODER_COUNT];
static volatile int8_t steps[ENCODER_COUNT]; // detents since the main loop last looked

// ENCODER_READ_STATE() takes a constant on some ports, so the encoders are sampled one by one
static void sample_one(uint8_t index, uint8_t state)
{
    const int8_t move = delta[(uint8_t)((last[index] << 2) | state)];

    last[index] = state;
    if (!move) {
        return;
    }

    position[index] += move;
    if (position[index] >= ENCODER_RESOLUTION) {
        position[index] -= ENCODER_RESOLUTION;
        steps[index]++;
    } else if (position[index] <= -ENCODER_RESOLUTION) {
        position[index] += ENCODER_RESOLUTION;
        steps[index]--;
    }
}

void encoder_sample(void)
{
    static bool primed;

    if (!primed) {
        for (uint8_t i = 0; i < ENCODER_COUNT; i++) {
            last[i] = ENCODER_READ_STATE(i);
        }
        primed = true;
        return;
    }

    for (uint8_t i = 0; i < ENCODER_COUNT; i++) {
        sample_one(i, ENCODER_READ_STATE(i));
    }
}

#define MAX_TAPS_PER_PASS 8 // a fast spin is worked off over a few passes of the main loop

void encoder_task(void)
{
    for (uint8_t i = 0; i < ENCODER_COUNT; i++) {
        int8_t pending;

        __critical
        {
            pending  = steps[i];
            steps[i] = 0;
        }

        for (uint8_t taps = 0; pending != 0 && taps < MAX_TAPS_PER_PASS; taps++) {
            const bool clockwise = pending > 0;

            matrix_tap_keycode(layer_lookup_encoder(i, clockwise));
            pending += clockwise ? -1 : 1;
        }

        if (pending != 0) { // keep what was not done for the next pass
            __critical
            {
                steps[i] += pending;
            }
        }
    }
}

#endif // ENCODER_COUNT
