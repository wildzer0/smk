// Host unit tests for src/smk/layer.c: built with the host compiler by
// tests/test_layer_host.py. The board is a 2x3 stand-in (tests/host/inc/kbdef.h).

#include "layer.h"
#include "keycodes.h"
#include <stdio.h>

#define T KC_TRANSPARENT

static const uint16_t map[4][2][3] = {
    /* 0 */ { { KC_A, KC_B, MO(1) }, { KC_C, KC_D, OSL(2) } },
    /* 1 */ { { KC_1, TG(3), TO(3) }, { KC_NO, KC_2, DF(1) } },
    /* 2 */ { { KC_F1, OSM(MOD_LSFT), T }, { T, T, T } },
    /* 3 */ { { KC_X, KC_Y, KC_Z }, { T, T, T } },
};

static uint8_t layers = 4;

uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    return map[layer][row][col];
}

uint8_t keymap_layer_count(void)
{
    return layers;
}

static uint8_t mods;

void layer_hook_mods_add(uint8_t m)
{
    mods |= m;
}

void layer_hook_mods_del(uint8_t m)
{
    mods &= (uint8_t)~m;
}

static int failures;
static int checks;

#define CHECK(cond)                                                   \
    do {                                                              \
        checks++;                                                     \
        if (!(cond)) {                                                \
            failures++;                                               \
            printf("FAIL line %d: %s\n", __LINE__, #cond);            \
        }                                                             \
    } while (0)

// Press/release a key the way matrix.c does; returns the keycode acted on.
static uint16_t press(uint8_t row, uint8_t col)
{
    const uint16_t kc = layer_key_down(row, col);
    if (!layer_process(kc, true)) {
        layer_key_pressed();
    }
    return kc;
}

static uint16_t release(uint8_t row, uint8_t col)
{
    const uint16_t kc = layer_key_up(row, col);
    if (!layer_process(kc, false)) {
        layer_key_released();
    }
    return kc;
}

static void reset(void)
{
    layer_init();
    mods = 0;
    layers = 4;
}

static void test_base_and_transparent(void)
{
    reset();
    CHECK(layer_lookup(0, 0) == KC_A);
    CHECK(layer_lookup(1, 1) == KC_D);
    layer_set_default(3);
    CHECK(layer_lookup(0, 0) == KC_X);
    CHECK(layer_lookup(1, 0) == KC_NO); // transparent on the default layer: nothing underneath
}

static void test_momentary(void)
{
    reset();
    CHECK(press(0, 2) == MO(1));
    CHECK(layer_lookup(0, 0) == KC_1);
    CHECK(layer_lookup(0, 1) == TG(3));
    CHECK(layer_lookup(1, 0) == KC_NO); // KC_NO on a layer is not transparent
    CHECK(release(0, 2) == MO(1));
    CHECK(layer_lookup(0, 0) == KC_A);
}

static void test_release_uses_press_time_keycode(void)
{
    reset();
    press(0, 2);                         // MO(1) down
    CHECK(press(0, 0) == KC_1);          // key pressed on layer 1
    release(0, 2);                       // layer off first
    CHECK(layer_lookup(0, 0) == KC_A);   // the layers now say KC_A ...
    CHECK(release(0, 0) == KC_1);        // ... but the release undoes KC_1
}

static void test_layer_priority(void)
{
    reset();
    layer_process(TG(3), true); // 3 on
    layer_process(MO(1), true); // 1 on as well
    CHECK(layer_lookup(0, 0) == KC_X); // the higher layer wins
    CHECK(layer_lookup(1, 1) == KC_2); // layer 3 is transparent here, layer 1 answers
    layer_process(MO(1), false);
    layer_process(TG(3), true); // toggled off
    CHECK(layer_lookup(0, 0) == KC_A);
}

static void test_toggle_to_default(void)
{
    reset();
    layer_process(TG(3), true);
    layer_process(TG(3), false); // release does nothing
    CHECK(layer_lookup(0, 0) == KC_X);
    layer_process(TG(3), true);
    CHECK(layer_lookup(0, 0) == KC_A);

    layer_process(MO(1), true);
    layer_process(TO(3), true); // TO clears the others, MO included
    CHECK(layer_lookup(0, 0) == KC_X);
    layer_process(MO(1), false); // releasing the MO of a layer that is already off is harmless
    CHECK(layer_lookup(0, 0) == KC_X);
    layer_process(TG(3), true);
    CHECK(layer_lookup(0, 0) == KC_A);

    layer_process(DF(1), true);
    CHECK(layer_get_default() == 1);
    CHECK(layer_lookup(0, 0) == KC_1);
}

static void test_invalid_layers_are_ignored(void)
{
    reset();
    layer_process(MO(9), true);  // beyond LAYER_MAX
    layer_process(TG(5), true);  // beyond the 4 layers of this keymap
    layer_process(OSL(7), true);
    layer_process(DF(6), true);
    CHECK(layer_lookup(0, 0) == KC_A);
    CHECK(layer_get_default() == 0);
}

static void test_one_shot_layer_tap(void)
{
    reset();
    press(1, 2);   // OSL(2)
    CHECK(layer_lookup(0, 0) == KC_F1);
    release(1, 2); // tapped: stays armed
    CHECK(layer_lookup(0, 0) == KC_F1);
    CHECK(press(0, 0) == KC_F1); // used by the next key ...
    CHECK(layer_lookup(0, 0) == KC_A); // ... and gone right away
    CHECK(release(0, 0) == KC_F1);
}

static void test_one_shot_layer_hold(void)
{
    reset();
    press(1, 2);
    CHECK(press(0, 0) == KC_F1);
    release(0, 0);
    CHECK(layer_lookup(0, 0) == KC_F1); // still held: stays on for more keys
    CHECK(press(0, 0) == KC_F1);
    release(0, 0);
    release(1, 2);
    CHECK(layer_lookup(0, 0) == KC_A); // dropped on release because it was used
}

static void test_one_shot_mod_from_a_layer(void)
{
    reset();
    layer_process(OSL(2), true);
    layer_process(OSL(2), false); // layer 2 armed
    press(0, 1);                  // OSM(MOD_LSFT) sits on layer 2
    CHECK(mods == MOD_LSFT);
    release(0, 1);                // tapped: shift stays on
    CHECK(mods == MOD_LSFT);
    CHECK(layer_lookup(0, 0) == KC_F1); // layer and one-shot keys do not use up an armed OSL
}

static void test_osm_applies_to_next_key(void)
{
    reset();
    layer_process(OSM(MOD_LSFT), true);
    CHECK(mods == 0x02);
    layer_process(OSM(MOD_LSFT), false); // tapped
    CHECK(mods == 0x02);
    press(0, 0);                         // next key goes out with shift ...
    CHECK(mods == 0x02);
    release(0, 0);                       // ... and shift is dropped when it is released
    CHECK(mods == 0);
}

static void test_osm_hold(void)
{
    reset();
    layer_process(OSM(MOD_LSFT), true);
    press(0, 0);
    release(0, 0);
    press(1, 0);
    release(1, 0);
    CHECK(mods == 0x02);                 // held through several keys
    layer_process(OSM(MOD_LSFT), false);
    CHECK(mods == 0);                    // used while held: no one-shot left over
    press(0, 0);
    CHECK(mods == 0);
    release(0, 0);
}

static void test_osm_right_hand(void)
{
    reset();
    layer_process(OSM(MOD_RCTL | MOD_RALT), true);
    CHECK(mods == 0x50); // right ctrl (0x10) + right alt (0x40)
    layer_process(OSM(MOD_RCTL | MOD_RALT), false);
    press(0, 0);
    release(0, 0);
    CHECK(mods == 0);
}

int main(void)
{
    test_base_and_transparent();
    test_momentary();
    test_release_uses_press_time_keycode();
    test_layer_priority();
    test_toggle_to_default();
    test_invalid_layers_are_ignored();
    test_one_shot_layer_tap();
    test_one_shot_layer_hold();
    test_one_shot_mod_from_a_layer();
    test_osm_applies_to_next_key();
    test_osm_hold();
    test_osm_right_hand();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
