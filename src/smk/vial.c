#include "vial.h"
#include "timer.h"
#include "matrix.h"
#include "keymap.h"
#include "layer.h"
#include "kbdef.h"
#include "keymap_store.h"
#include <string.h>

#define UNLOCK_COUNTER_MAX 50u
#define UNLOCK_STEP_MS     100u

static bool     unlocked;
static bool     unlock_in_progress;
static uint8_t  unlock_counter;
static uint16_t unlock_timer;

bool vial_is_unlocked(void)
{
    return unlocked;
}

bool vial_unlock_in_progress(void)
{
    return unlock_in_progress;
}

static bool unlock_keys_held(void)
{
    for (uint8_t i = 0; i < vial_unlock_key_count; i++) {
        if (!matrix_is_on(vial_unlock_keys[i][0], vial_unlock_keys[i][1])) {
            return false;
        }
    }

    return true;
}

void vial_receive(uint8_t *data, uint8_t length)
{
    switch (data[1]) {
        case vial_get_keyboard_id:
            memset(data, 0, length);
            data[0] = (uint8_t)(VIAL_PROTOCOL_VERSION & 0xFFu);
            data[1] = (uint8_t)((VIAL_PROTOCOL_VERSION >> 8) & 0xFFu);
            data[2] = (uint8_t)((VIAL_PROTOCOL_VERSION >> 16) & 0xFFu);
            data[3] = (uint8_t)((VIAL_PROTOCOL_VERSION >> 24) & 0xFFu);
            for (uint8_t i = 0; i < 8; i++) {
                data[4 + i] = vial_keyboard_uid[i];
            }
            break;

        case vial_get_size:
            data[0] = (uint8_t)(vial_definition_size & 0xFFu);
            data[1] = (uint8_t)((vial_definition_size >> 8) & 0xFFu);
            data[2] = (uint8_t)((vial_definition_size >> 16) & 0xFFu);
            data[3] = (uint8_t)((vial_definition_size >> 24) & 0xFFu);
            break;

        // 32-byte pages of the compressed definition; a page past the end gets no reply content (the request is echoed)
        case vial_get_def: {
            const uint32_t start = (uint32_t)(data[2] | ((uint16_t)data[3] << 8)) * length;
            uint32_t       end   = start + length;

            if (start >= vial_definition_size) {
                break;
            }
            if (end > vial_definition_size) {
                end = vial_definition_size;
            }
            for (uint32_t i = start; i < end; i++) {
                data[i - start] = vial_definition[i];
            }
            break;
        }

#ifdef ENCODER_COUNT
        // The GUI asks for both directions at once: counter-clockwise first, then clockwise (big-endian keycodes).
        case vial_get_encoder: {
            const bool valid = data[2] < keymap_layer_count() && data[3] < ENCODER_COUNT;
            const uint16_t ccw = valid ? keymap_encoder_get(data[2], data[3], false) : 0;
            const uint16_t cw  = valid ? keymap_encoder_get(data[2], data[3], true) : 0;

            data[0] = (uint8_t)(ccw >> 8);
            data[1] = (uint8_t)(ccw & 0xFFu);
            data[2] = (uint8_t)(cw >> 8);
            data[3] = (uint8_t)(cw & 0xFFu);
            break;
        }

        // layer, encoder, direction (0 = counter-clockwise, 1 = clockwise), keycode
        case vial_set_encoder:
            keymap_encoder_set(data[2], data[3], data[4] != 0, (uint16_t)(((uint16_t)data[5] << 8) | data[6]));
            break;
#endif

        case vial_get_unlock_status:
            memset(data, 0xFF, length);
            data[0] = unlocked;
            data[1] = unlock_in_progress;
            for (uint8_t i = 0; i < vial_unlock_key_count; i++) {
                data[2 + i * 2]     = vial_unlock_keys[i][0];
                data[2 + i * 2 + 1] = vial_unlock_keys[i][1];
            }
            break;

        case vial_unlock_start:
            unlock_in_progress = true;
            unlock_counter     = UNLOCK_COUNTER_MAX;
            unlock_timer       = timer_read();
            break;

        // the GUI polls while the user holds the keys; they must stay down for UNLOCK_COUNTER_MAX steps of 100 ms
        case vial_unlock_poll:
            if (unlock_in_progress) {
                if (timer_elapsed(unlock_timer) > UNLOCK_STEP_MS && unlock_keys_held()) {
                    unlock_timer = timer_read();

                    if (--unlock_counter == 0) {
                        unlock_in_progress = false;
                        unlocked           = true;
                    }
                } else {
                    unlock_counter = UNLOCK_COUNTER_MAX;
                }
            }
            data[0] = unlocked;
            data[1] = unlock_in_progress;
            data[2] = unlock_counter;
            break;

        case vial_lock:
            unlocked = false;
            break;

        // no QMK settings: all 0xFF tells the GUI there is none
        case vial_qmk_settings_query:
            memset(data, 0xFF, length);
            break;

        // tap dances (combos and key overrides: none). 0x00 counts the entries, 0x01 reads one (status, then its five words),
        // 0x02 writes one (index in data[3], the five words from data[4])
        case vial_dynamic_entry_op:
            switch (data[2]) {
                case 0x00:
                    memset(data, 0, length);
#ifdef KEYMAP_TAP_DANCE_COUNT
                    data[0] = KEYMAP_TAP_DANCE_COUNT;
#endif
                    break;
#ifdef KEYMAP_TAP_DANCE_COUNT
                case 0x01: {
                    const uint8_t index = data[3];
                    memset(data, 0, length);
                    if (index >= KEYMAP_TAP_DANCE_COUNT) {
                        data[0] = 1;
                        break;
                    }
                    for (uint8_t field = 0; field < 5; field++) {
                        const uint16_t word = keymap_store_tap_dance_get(index, field);
                        data[1 + field * 2]     = (uint8_t)(word & 0xFFu);
                        data[2 + field * 2]     = (uint8_t)(word >> 8);
                    }
                    break;
                }
                case 0x02: {
                    const uint8_t index = data[3];
                    uint8_t       status = (index >= KEYMAP_TAP_DANCE_COUNT) ? 1 : 0;
                    if (!status) {
                        for (uint8_t field = 0; field < 5; field++) {
                            keymap_store_tap_dance_set(index, field, (uint16_t)(data[4 + field * 2] | ((uint16_t)data[5 + field * 2] << 8)));
                        }
                    }
                    data[0] = status;
                    break;
                }
#endif
                default:
                    break;
            }
            break;

        // encoders, QMK settings get/set/reset: the keyboard has none, the request is echoed back
        default:
            break;
    }
}
