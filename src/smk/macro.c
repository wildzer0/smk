#include "macro.h"

#ifdef KEYMAP_MACRO_BYTES

#include "keymap_store.h"
#include "keycodes.h"

// the action codes after the 0x01 prefix
#define MACRO_PREFIX 0x01
#define ACTION_TAP   0x01
#define ACTION_DOWN  0x02
#define ACTION_UP    0x03
#define ACTION_DELAY 0x04
#define EXT_TAP      0x05 // the same three with a 16-bit keycode
#define EXT_DOWN     0x06
#define EXT_UP       0x07

// US layout: HID usage for 0x20..0x7E, bit 7 = needs shift
#define SH 0x80u
static const __code uint8_t ascii_to_hid[95] = {
    0x2C,                                                                            // space
    SH | 0x1E, SH | 0x34, SH | 0x20, SH | 0x21, SH | 0x22, SH | 0x24,                // ! " # $ % &
    0x34,                                                                            // '
    SH | 0x26, SH | 0x27, SH | 0x25, SH | 0x2E,                                      // ( ) * +
    0x36, 0x2D, 0x37, 0x38,                                                          // , - . /
    0x27, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,                      // 0-9
    SH | 0x33, 0x33, SH | 0x36, 0x2E, SH | 0x37, SH | 0x38, SH | 0x1F,              // : ; < = > ? @
    SH | 0x04, SH | 0x05, SH | 0x06, SH | 0x07, SH | 0x08, SH | 0x09, SH | 0x0A, SH | 0x0B, SH | 0x0C, SH | 0x0D, SH | 0x0E, SH | 0x0F, SH | 0x10,
    SH | 0x11, SH | 0x12, SH | 0x13, SH | 0x14, SH | 0x15, SH | 0x16, SH | 0x17, SH | 0x18, SH | 0x19, SH | 0x1A, SH | 0x1B, SH | 0x1C, SH | 0x1D, // A-Z
    0x2F, 0x31, 0x30, SH | 0x23, SH | 0x2D, 0x35,                                    // [ \ ] ^ _ `
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,    // a-z
    SH | 0x2F, SH | 0x31, SH | 0x30, SH | 0x35,                                      // { | } ~
};

static uint16_t        position; // where the next byte is read from
static __xdata uint8_t one;      // a single byte read from the buffer

static uint8_t read_byte(void)
{
    one = 0;

    if (position < KEYMAP_MACRO_BYTES) {
        keymap_store_macro_read(position, 1, &one);
    }
    position++;
    return one;
}

static void tap(uint16_t keycode)
{
    macro_hook_press(keycode);
    macro_hook_release(keycode);
}

static void type_character(uint8_t c)
{
    uint8_t hid = 0;
    bool    shifted = false;

    switch (c) {
        case '\b':
            hid = KC_BSPC;
            break;
        case '\t':
            hid = KC_TAB;
            break;
        case '\n':
            hid = KC_ENT;
            break;
        case 0x1B:
            hid = KC_ESC;
            break;
        case 0x7F:
            hid = KC_DEL;
            break;
        default:
            if (c >= 0x20 && c <= 0x7E) {
                hid     = ascii_to_hid[c - 0x20] & 0x7F;
                shifted = (ascii_to_hid[c - 0x20] & SH) != 0;
            }
            break;
    }

    if (!hid) {
        return;
    }
    if (shifted) {
        macro_hook_press(KC_LSFT);
    }
    tap(hid);
    if (shifted) {
        macro_hook_release(KC_LSFT);
    }
}

// A 16-bit keycode cannot hold a 0 byte (it would end the macro), so Vial stores 0x0100 as 0xFF01, 0x0200 as 0xFF02, ...
static uint16_t decode_keycode(uint16_t keycode)
{
    return (keycode > 0xFF00u) ? (uint16_t)((keycode & 0xFFu) << 8) : keycode;
}

void macro_play(uint8_t id)
{
    if (id >= KEYMAP_MACRO_COUNT) {
        return;
    }

    // a buffer whose last byte is not 0 is one being rewritten, or damaged: play nothing
    keymap_store_macro_read(KEYMAP_MACRO_BYTES - 1, 1, &one);
    if (one != 0) {
        return;
    }

    // skip to the id-th macro: they are separated by 0 bytes
    position = 0;
    while (id > 0) {
        if (position >= KEYMAP_MACRO_BYTES) {
            return;
        }
        if (read_byte() == 0) {
            id--;
        }
    }

    for (;;) {
        const uint8_t c = read_byte();

        if (c == 0 || position > KEYMAP_MACRO_BYTES) {
            break;
        }

        if (c != MACRO_PREFIX) {
            type_character(c);
            continue;
        }

        const uint8_t action = read_byte();
        if (action == 0) {
            break;
        }

        if (action == ACTION_TAP || action == ACTION_DOWN || action == ACTION_UP) {
            const uint8_t key = read_byte();
            if (key) {
                if (action == ACTION_TAP) {
                    tap(key);
                } else if (action == ACTION_DOWN) {
                    macro_hook_press(key);
                } else {
                    macro_hook_release(key);
                }
            }
        } else if (action == EXT_TAP || action == EXT_DOWN || action == EXT_UP) {
            const uint8_t low = read_byte();
            if (low) {
                const uint8_t high = read_byte();
                if (high) {
                    const uint16_t keycode = decode_keycode((uint16_t)(low | ((uint16_t)high << 8)));
                    if (action == EXT_TAP) {
                        tap(keycode);
                    } else if (action == EXT_DOWN) {
                        macro_hook_press(keycode);
                    } else {
                        macro_hook_release(keycode);
                    }
                }
            }
        } else if (action == ACTION_DELAY) {
            const uint8_t d0 = read_byte();
            const uint8_t d1 = read_byte();
            if (d0 == 0 || d1 == 0) {
                break;
            }
            // 0 cannot be stored, so both bytes are kept one higher
            macro_hook_wait_ms((uint16_t)((d0 - 1) + (d1 - 1) * 255));
        }
    }
}

#endif // KEYMAP_MACRO_BYTES
