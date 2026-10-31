#include "keymap_store.h"
#include "kbdef.h" // the host tests define FLASH_KEYMAP_* here; the firmware gets them from meson

#ifdef FLASH_KEYMAP_ADDR

#include "flash.h"

#define LAYERS     KEYMAP_DYNAMIC_LAYERS
#define KEY_WORDS  ((uint16_t)(LAYERS * MATRIX_ROWS * MATRIX_COLS))
#ifdef ENCODER_COUNT
#    define ENCODERS      ENCODER_COUNT
#    define ENCODER_WORDS ((uint16_t)(LAYERS * ENCODER_COUNT * 2))
#else
#    define ENCODERS      0
#    define ENCODER_WORDS 0
#endif
#ifdef KEYMAP_MACRO_BYTES
#    define MACRO_BYTES KEYMAP_MACRO_BYTES
#    define MACRO_WORDS ((uint16_t)((KEYMAP_MACRO_BYTES + 1) / 2))
_Static_assert(KEYMAP_MACRO_BYTES % 2 == 0 && KEYMAP_MACRO_BYTES / 8 < 256, "the macro buffer is a whole number of words and its size must fit a header byte");
#else
#    define MACRO_BYTES 0
#    define MACRO_WORDS 0
#endif
#ifdef KEYMAP_TAP_DANCE_COUNT
#    define TD_WORDS ((uint16_t)(KEYMAP_TAP_DANCE_COUNT * 5)) // on tap, on hold, on double tap, on tap-hold, tapping term
#else
#    define TD_WORDS 0
#endif
#define WORDS      ((uint16_t)(KEY_WORDS + ENCODER_WORDS + MACRO_WORDS + TD_WORDS)) // keys, then the encoders, the macros, the tap dances
#define PAYLOAD    ((uint16_t)(WORDS * 2u))
#define HEADER     14u
#define SLOT_BYTES ((uint16_t)(FLASH_KEYMAP_SECTORS * FLASH_KEYMAP_SECTOR_SIZE))
#define CHUNK      ((uint16_t)64u)

#define MAGIC0  'K'
#define MAGIC1  'M'
#define VERSION 4u

#define NO_SLOT 0xFFu

_Static_assert(LAYERS >= 1 && LAYERS <= 8, "the layer engine addresses at most 8 layers");
_Static_assert((uint32_t)HEADER + PAYLOAD <= SLOT_BYTES, "the keymap does not fit in a slot: raise keymap_sectors");

static __xdata uint16_t shadow[LAYERS][MATRIX_ROWS][MATRIX_COLS];
#ifdef ENCODER_COUNT
static __xdata uint16_t encoder_shadow[LAYERS][ENCODER_COUNT][2]; // [ccw, cw]
#endif
#ifdef KEYMAP_MACRO_BYTES
static __xdata uint16_t macro_shadow[MACRO_WORDS]; // the buffer's bytes, little-endian in each word
#endif
#ifdef KEYMAP_TAP_DANCE_COUNT
static __xdata uint16_t td_shadow[KEYMAP_TAP_DANCE_COUNT][5];
#endif
static __xdata uint8_t  chunk[CHUNK];

static bool     dirty;
static uint8_t  active_slot = NO_SLOT;
static uint16_t active_seq;

// the header just read by slot_header(); statics rather than out-parameters, as the
// firmware keeps its locals on a small stack
static uint16_t header_seq;
static uint16_t header_crc;

// Fingerprint of the compiled-in keymap (keys and encoder), kept in the header: a copy saved by a firmware whose default keymap
// was different (a new key on the Fn layer, say) is not used, because it would hide what the new firmware adds.
static uint16_t defaults_print;

static uint16_t slot_addr(uint8_t slot)
{
    return (uint16_t)(FLASH_KEYMAP_ADDR + (uint16_t)slot * SLOT_BYTES);
}

// CRC-16/CCITT-FALSE, bitwise: slow but tiny, and it only runs at boot and on a save.
static uint16_t crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static void compute_defaults_print(void)
{
    uint16_t crc = 0xFFFFu;

    for (uint8_t layer = 0; layer < LAYERS; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                const uint16_t word = keymap_default(layer, row, col);
                crc                 = crc16_update(crc, (uint8_t)(word & 0xFFu));
                crc                 = crc16_update(crc, (uint8_t)(word >> 8));
            }
        }
#ifdef ENCODER_COUNT
        for (uint8_t encoder = 0; encoder < ENCODER_COUNT; encoder++) {
            for (uint8_t way = 0; way < 2; way++) {
                const uint16_t word = keymap_default_encoder(layer, encoder, way != 0);
                crc                 = crc16_update(crc, (uint8_t)(word & 0xFFu));
                crc                 = crc16_update(crc, (uint8_t)(word >> 8));
            }
        }
#endif
    }

#ifdef KEYMAP_TAP_DANCE_COUNT
    for (uint8_t i = 0; i < KEYMAP_TAP_DANCE_COUNT; i++) {
        for (uint8_t field = 0; field < 5; field++) {
            const uint16_t word = keymap_default_tap_dance(i, field);
            crc                 = crc16_update(crc, (uint8_t)(word & 0xFFu));
            crc                 = crc16_update(crc, (uint8_t)(word >> 8));
        }
    }
#endif
    defaults_print = crc;
}

// The payload as one run of words: the keys, then the encoders, then the macros.
static __xdata uint16_t *word_at(uint16_t index)
{
#ifdef KEYMAP_TAP_DANCE_COUNT
    if (index >= KEY_WORDS + ENCODER_WORDS + MACRO_WORDS) {
        return &td_shadow[0][0] + (index - KEY_WORDS - ENCODER_WORDS - MACRO_WORDS);
    }
#endif
#ifdef KEYMAP_MACRO_BYTES
    if (index >= KEY_WORDS + ENCODER_WORDS) {
        return &macro_shadow[0] + (index - KEY_WORDS - ENCODER_WORDS);
    }
#endif
#ifdef ENCODER_COUNT
    if (index >= KEY_WORDS) {
        return &encoder_shadow[0][0][0] + (index - KEY_WORDS);
    }
#endif
    return &shadow[0][0][0] + index;
}

// `macros` also empties the macro buffer: wanted when nothing valid was stored, not when only the keymap is reset.
static void load_defaults(bool macros)
{
    for (uint8_t layer = 0; layer < LAYERS; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                shadow[layer][row][col] = keymap_default(layer, row, col);
            }
        }
#ifdef ENCODER_COUNT
        for (uint8_t encoder = 0; encoder < ENCODER_COUNT; encoder++) {
            encoder_shadow[layer][encoder][0] = keymap_default_encoder(layer, encoder, false);
            encoder_shadow[layer][encoder][1] = keymap_default_encoder(layer, encoder, true);
        }
#endif
    }
#ifdef KEYMAP_TAP_DANCE_COUNT
    for (uint8_t i = 0; i < KEYMAP_TAP_DANCE_COUNT; i++) {
        for (uint8_t field = 0; field < 5; field++) {
            td_shadow[i][field] = keymap_default_tap_dance(i, field);
        }
    }
#endif
#ifdef KEYMAP_MACRO_BYTES
    if (macros) {
        for (uint16_t i = 0; i < MACRO_WORDS; i++) {
            macro_shadow[i] = 0;
        }
    }
#else
    (void)macros;
#endif
}

// The payload is little-endian words, row-major per layer.
static uint16_t shadow_crc(void)
{
    uint16_t crc = 0xFFFFu;

    for (uint16_t i = 0; i < WORDS; i++) {
        const uint16_t word = *word_at(i);
        crc                 = crc16_update(crc, (uint8_t)(word & 0xFFu));
        crc                 = crc16_update(crc, (uint8_t)(word >> 8));
    }

    return crc;
}

// Reads the header of `slot`; true when it is a complete keymap of this build's shape.
static bool slot_header(uint8_t slot)
{
    flash_read_into(FLASH_CODE, slot_addr(slot), chunk, HEADER);

    if (chunk[0] != MAGIC0 || chunk[1] != MAGIC1 || chunk[2] != VERSION || chunk[3] != LAYERS || chunk[4] != MATRIX_ROWS || chunk[5] != MATRIX_COLS || chunk[10] != ENCODERS || chunk[11] != (MACRO_BYTES / 8) || chunk[12] != (uint8_t)(defaults_print & 0xFFu) || chunk[13] != (uint8_t)(defaults_print >> 8)) {
        return false;
    }

    header_seq = (uint16_t)(chunk[6] | ((uint16_t)chunk[7] << 8));
    header_crc = (uint16_t)(chunk[8] | ((uint16_t)chunk[9] << 8));
    return true;
}

// Reads the payload of `slot` into the RAM copy; returns its CRC.
static uint16_t slot_read_payload(uint8_t slot)
{
    uint16_t crc = 0xFFFFu;
    uint16_t at  = 0; // byte offset into the payload

    while (at < PAYLOAD) {
        const uint8_t n = (PAYLOAD - at > CHUNK) ? (uint8_t)CHUNK : (uint8_t)(PAYLOAD - at);

        flash_read_into(FLASH_CODE, (uint16_t)(slot_addr(slot) + HEADER + at), chunk, n);

        for (uint8_t i = 0; i < n; i++) {
            crc = crc16_update(crc, chunk[i]);
        }
        for (uint8_t i = 0; i < n; i += 2) {
            *word_at((uint16_t)((at + i) / 2)) = (uint16_t)(chunk[i] | ((uint16_t)chunk[i + 1] << 8));
        }

        at += n;
    }

    return crc;
}

// True when `slot` holds a complete keymap; its sequence number is then in header_seq.
static bool slot_valid(uint8_t slot)
{
    return slot_header(slot) && slot_read_payload(slot) == header_crc;
}

void keymap_store_init(void)
{
    uint16_t seq0 = 0;
    uint16_t seq1 = 0;

    compute_defaults_print();

    const bool valid0 = slot_valid(0);
    if (valid0) {
        seq0 = header_seq;
    }
    const bool valid1 = slot_valid(1);
    if (valid1) {
        seq1 = header_seq;
    }

    dirty       = false;
    active_slot = NO_SLOT;

    if (!valid0 && !valid1) {
        load_defaults(true);
        return;
    }

    // wrap-safe "newer": the sequence is only ever compared between two live slots
    const uint8_t newest = (!valid0 || (valid1 && (int16_t)(seq1 - seq0) > 0)) ? 1 : 0;

    slot_read_payload(newest); // the last slot read is not necessarily the one that wins
    active_slot = newest;
    active_seq  = newest ? seq1 : seq0;
}

uint16_t keymap_store_get(uint8_t layer, uint8_t row, uint8_t col)
{
    return shadow[layer][row][col];
}

bool keymap_store_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    if (layer >= LAYERS || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return false;
    }

    if (shadow[layer][row][col] != keycode) {
        shadow[layer][row][col] = keycode;
        dirty                   = true;
    }

    return true;
}

#ifdef ENCODER_COUNT
uint16_t keymap_store_get_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    return encoder_shadow[layer][encoder][clockwise ? 1 : 0];
}

bool keymap_store_set_encoder(uint8_t layer, uint8_t encoder, bool clockwise, uint16_t keycode)
{
    if (layer >= LAYERS || encoder >= ENCODER_COUNT) {
        return false;
    }

    if (encoder_shadow[layer][encoder][clockwise ? 1 : 0] != keycode) {
        encoder_shadow[layer][encoder][clockwise ? 1 : 0] = keycode;
        dirty                                            = true;
    }

    return true;
}
#endif

#ifdef KEYMAP_MACRO_BYTES
void keymap_store_macro_read(uint16_t offset, uint8_t size, uint8_t *dst)
{
    for (uint8_t i = 0; i < size; i++) {
        const uint16_t at = (uint16_t)(offset + i);

        if (at >= MACRO_BYTES) {
            dst[i] = 0;
        } else {
            const uint16_t word = macro_shadow[at / 2];
            dst[i]              = (at & 1u) ? (uint8_t)(word >> 8) : (uint8_t)(word & 0xFFu);
        }
    }
}

void keymap_store_macro_write(uint16_t offset, uint8_t size, const uint8_t *src)
{
    for (uint8_t i = 0; i < size; i++) {
        const uint16_t at = (uint16_t)(offset + i);

        if (at >= MACRO_BYTES) {
            continue;
        }

        const uint16_t word = macro_shadow[at / 2];
        const uint16_t next = (at & 1u) ? (uint16_t)((word & 0x00FFu) | ((uint16_t)src[i] << 8)) : (uint16_t)((word & 0xFF00u) | src[i]);

        if (next != word) {
            macro_shadow[at / 2] = next;
            dirty                = true;
        }
    }
}

void keymap_store_macro_clear(void)
{
    for (uint16_t i = 0; i < MACRO_WORDS; i++) {
        if (macro_shadow[i]) {
            macro_shadow[i] = 0;
            dirty           = true;
        }
    }
}
#endif

#ifdef KEYMAP_TAP_DANCE_COUNT
uint16_t keymap_store_tap_dance_get(uint8_t index, uint8_t field)
{
    return (index < KEYMAP_TAP_DANCE_COUNT && field < 5) ? td_shadow[index][field] : 0;
}

bool keymap_store_tap_dance_set(uint8_t index, uint8_t field, uint16_t value)
{
    if (index >= KEYMAP_TAP_DANCE_COUNT || field >= 5) {
        return false;
    }

    if (td_shadow[index][field] != value) {
        td_shadow[index][field] = value;
        dirty                   = true;
    }

    return true;
}
#endif

void keymap_store_reset(void)
{
    load_defaults(false);
    dirty = true;
}

bool keymap_store_dirty(void)
{
    return dirty;
}

void keymap_store_flush(void)
{
    if (!dirty) {
        return;
    }
    dirty = false; // cleared first: an edit during the write is not lost

    const uint8_t  target = (active_slot == 0) ? 1 : 0;
    const uint16_t seq    = (uint16_t)(active_seq + 1);
    const uint16_t base   = slot_addr(target);

    for (uint8_t sector = 0; sector < FLASH_KEYMAP_SECTORS; sector++) {
        flash_erase(FLASH_CODE, (uint16_t)(base + (uint16_t)sector * FLASH_KEYMAP_SECTOR_SIZE));
    }

    uint16_t at = 0;
    while (at < PAYLOAD) {
        const uint8_t n = (PAYLOAD - at > CHUNK) ? (uint8_t)CHUNK : (uint8_t)(PAYLOAD - at);

        for (uint8_t i = 0; i < n; i += 2) {
            const uint16_t word = *word_at((uint16_t)((at + i) / 2));
            chunk[i]            = (uint8_t)(word & 0xFFu);
            chunk[i + 1]        = (uint8_t)(word >> 8);
        }
        flash_program_from(FLASH_CODE, (uint16_t)(base + HEADER + at), chunk, n);

        at += n;
    }

    // last: until this lands the slot is not a valid keymap
    const uint16_t crc = shadow_crc();
    chunk[0]           = MAGIC0;
    chunk[1]           = MAGIC1;
    chunk[2]           = VERSION;
    chunk[3]           = LAYERS;
    chunk[4]           = MATRIX_ROWS;
    chunk[5]           = MATRIX_COLS;
    chunk[6]           = (uint8_t)(seq & 0xFFu);
    chunk[7]           = (uint8_t)(seq >> 8);
    chunk[8]           = (uint8_t)(crc & 0xFFu);
    chunk[9]           = (uint8_t)(crc >> 8);
    chunk[10]          = ENCODERS;
    chunk[11]          = MACRO_BYTES / 8; // the macro buffer's size, in 8-byte units, so a size change invalidates old data
    chunk[12]          = (uint8_t)(defaults_print & 0xFFu);
    chunk[13]          = (uint8_t)(defaults_print >> 8);
    flash_program_from(FLASH_CODE, base, chunk, HEADER);

    active_slot = target;
    active_seq  = seq;
}

#endif // FLASH_KEYMAP_ADDR
