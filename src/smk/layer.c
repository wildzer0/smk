#include "layer.h"
#include "keycodes.h"
#include "kbdef.h"
#include "timer.h"
#include "tapping.h"

#define NO_LAYER 0xFFu

// the keycode each key resolved to when it went down
static uint16_t down_keycode[MATRIX_ROWS][MATRIX_COLS];

static uint8_t layer_state;   // bit n = layer n active
static uint8_t default_layer; // always part of the active set, tried last

// layer tap toggle (TT)
static uint8_t  tt_layer = NO_LAYER;
static uint8_t  tt_count;   // taps in a row
static uint16_t tt_down;    // when the key went down
static uint16_t tt_last;    // when it was last let go
static bool     tt_toggled; // the layer was latched on

// one-shot layer (OSL)
static uint8_t os_layer = NO_LAYER;
static bool    os_held;
static bool    os_used; // a key was pressed while the OSL key was down

// one-shot modifiers (OSM), HID modifier-byte bits
static uint8_t osm_held;    // OSM keys currently down
static uint8_t osm_used;    // of those, the ones a key was pressed under
static uint8_t osm_armed;   // tapped, waiting for the next key
static uint8_t osm_pending; // armed mods applied to a key that is still down

void layer_init(void)
{
    layer_state   = 0;
    default_layer = 0;
    os_layer      = NO_LAYER;
    tt_layer      = NO_LAYER;
    tt_count      = 0;
    tt_toggled    = false;
    os_held       = false;
    os_used       = false;
    osm_held      = 0;
    osm_used      = 0;
    osm_armed     = 0;
    osm_pending   = 0;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            down_keycode[row][col] = KC_NO;
        }
    }
}

static bool layer_valid(uint8_t layer)
{
    return layer < LAYER_MAX && layer < keymap_layer_count();
}

void layer_set_default(uint8_t layer)
{
    if (layer_valid(layer)) {
        default_layer = layer;
    }
}

uint8_t layer_get_default(void)
{
    return default_layer;
}

void layer_hold(uint8_t layer, bool on)
{
    if (!layer_valid(layer)) {
        return;
    }
    if (on) {
        layer_state |= (uint8_t)(1u << layer);
    } else {
        layer_state &= (uint8_t)~(1u << layer);
    }
}

static void layer_on(uint8_t layer)
{
    layer_state |= (uint8_t)(1u << layer);
}

static void layer_off(uint8_t layer)
{
    layer_state &= (uint8_t)~(1u << layer);
}

uint16_t layer_lookup(uint8_t row, uint8_t col)
{
    for (uint8_t layer = LAYER_MAX; layer-- > 0;) {
        if (!(layer_state & (uint8_t)(1u << layer)) || !layer_valid(layer)) {
            continue;
        }

        const uint16_t keycode = keymap_get(layer, row, col);
        if (keycode != KC_TRANSPARENT) {
            return keycode;
        }
    }

    const uint16_t keycode = keymap_get(default_layer, row, col);
    return (keycode == KC_TRANSPARENT) ? KC_NO : keycode;
}

uint16_t layer_key_down(uint8_t row, uint8_t col)
{
    const uint16_t keycode = layer_lookup(row, col);

    down_keycode[row][col] = keycode;
    return keycode;
}

uint16_t layer_key_up(uint8_t row, uint8_t col)
{
    const uint16_t keycode = down_keycode[row][col];

    down_keycode[row][col] = KC_NO;
    return keycode;
}

#ifdef ENCODER_COUNT
uint16_t layer_lookup_encoder(uint8_t encoder, bool clockwise)
{
    for (uint8_t layer = LAYER_MAX; layer-- > 0;) {
        if (!(layer_state & (uint8_t)(1u << layer)) || !layer_valid(layer)) {
            continue;
        }

        const uint16_t keycode = keymap_encoder_get(layer, encoder, clockwise);
        if (keycode != KC_TRANSPARENT) {
            return keycode;
        }
    }

    const uint16_t keycode = keymap_encoder_get(default_layer, encoder, clockwise);
    return (keycode == KC_TRANSPARENT) ? KC_NO : keycode;
}
#endif

// QMK encodes OSM as a 5-bit mask: the low 4 bits pick the modifiers, bit 4 moves them to the right hand.
static uint8_t osm_mods(uint16_t keycode)
{
    const uint8_t mask = (uint8_t)(keycode & 0x1F);
    return (mask & 0x10) ? (uint8_t)((mask & 0x0F) << 4) : (uint8_t)(mask & 0x0F);
}

static void osl_finish(void)
{
    if (os_layer != NO_LAYER) {
        layer_off(os_layer);
        os_layer = NO_LAYER;
    }
}

bool layer_process(uint16_t keycode, bool pressed)
{
    if (IS_QK_MOMENTARY(keycode)) {
        const uint8_t layer = QK_MOMENTARY_GET_LAYER(keycode);
        if (layer_valid(layer)) {
            if (pressed) {
                layer_on(layer);
            } else {
                layer_off(layer);
            }
        }
        return true;
    }

    if (IS_QK_TO(keycode)) {
        const uint8_t layer = (uint8_t)(keycode & 0x1F);
        if (pressed && layer_valid(layer)) {
            layer_state = (uint8_t)(1u << layer);
            os_layer    = NO_LAYER;
        }
        return true;
    }

    if (IS_QK_TOGGLE_LAYER(keycode)) {
        const uint8_t layer = (uint8_t)(keycode & 0x1F);
        if (pressed && layer_valid(layer)) {
            layer_state ^= (uint8_t)(1u << layer);
        }
        return true;
    }

    if (IS_QK_DEF_LAYER(keycode)) {
        if (pressed) {
            layer_set_default((uint8_t)(keycode & 0x1F));
        }
        return true;
    }

    if (IS_QK_ONE_SHOT_LAYER(keycode)) {
        const uint8_t layer = (uint8_t)(keycode & 0x1F);
        if (!layer_valid(layer)) {
            return true;
        }

        if (pressed) {
            osl_finish(); // an earlier one-shot still waiting is dropped
            layer_on(layer);
            os_layer = layer;
            os_held  = true;
            os_used  = false;
        } else if (os_layer == layer) {
            os_held = false;
            if (os_used) {
                osl_finish();
            }
        }
        return true;
    }

    if (IS_QK_LAYER_MOD(keycode)) {
        const uint8_t layer = (uint8_t)QK_LAYER_MOD_GET_LAYER(keycode);
        const uint8_t mods  = osm_mods((uint16_t)QK_LAYER_MOD_GET_MODS(keycode));

        if (layer_valid(layer)) {
            if (pressed) {
                layer_on(layer);
                layer_hook_mods_add(mods);
            } else {
                layer_off(layer);
                layer_hook_mods_del(mods);
            }
        }
        return true;
    }

    if (IS_QK_LAYER_TAP_TOGGLE(keycode)) {
        const uint8_t layer = (uint8_t)(keycode & 0x1F);

        if (!layer_valid(layer)) {
            return true;
        }

        if (pressed) {
            if (tt_layer != layer || timer_elapsed(tt_last) > TAPPING_TERM) {
                tt_count = 0; // a pause or another layer starts the count over
            }
            if (tt_layer != layer) {
                tt_toggled = false;
            }
            tt_layer = layer;
            tt_down  = timer_read();
            layer_on(layer);
        } else {
            tt_count = (timer_elapsed(tt_down) < TAPPING_TERM) ? (uint8_t)(tt_count + 1) : 0;
            tt_last  = timer_read();

            if (tt_count >= TAPPING_TOGGLE) {
                tt_count   = 0;
                tt_toggled = !tt_toggled;
            }
            if (!tt_toggled) {
                layer_off(layer);
            }
        }
        return true;
    }

    if (IS_QK_ONE_SHOT_MOD(keycode)) {
        const uint8_t mods = osm_mods(keycode);

        if (pressed) {
            layer_hook_mods_add(mods);
            osm_held |= mods;
        } else {
            osm_held &= (uint8_t)~mods;
            if (osm_used & mods) {
                osm_used &= (uint8_t)~mods;
                layer_hook_mods_del(mods);
            } else {
                osm_armed |= mods; // tapped: the modifier stays on for the next key
            }
        }
        return true;
    }

    return false;
}

void layer_key_pressed(void)
{
    if (os_layer != NO_LAYER) {
        if (os_held) {
            os_used = true; // used under the held OSL key: dropped when it is released
        } else {
            osl_finish(); // tapped OSL: this key was the one it was waiting for
        }
    }

    osm_used |= osm_held;

    if (osm_armed) {
        osm_pending |= osm_armed; // stays on until this key is released
        osm_armed = 0;
    }
}

void layer_key_released(void)
{
    if (osm_pending) {
        layer_hook_mods_del(osm_pending);
        osm_pending = 0;
    }
}
