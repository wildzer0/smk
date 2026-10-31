#include "keymap_store.h"
#include "kbdef.h" // the host tests define FLASH_KEYMAP_* here; the firmware gets them from meson

#ifdef FLASH_KEYMAP_ADDR

#include "flash.h"

#define LAYERS     KEYMAP_DYNAMIC_LAYERS
#define WORDS      ((uint16_t)(LAYERS * MATRIX_ROWS * MATRIX_COLS))
#define PAYLOAD    ((uint16_t)(WORDS * 2u))
#define HEADER     12u // 10 bytes are written, the last two stay erased
#define SLOT_BYTES ((uint16_t)(FLASH_KEYMAP_SECTORS * FLASH_KEYMAP_SECTOR_SIZE))
#define CHUNK      ((uint16_t)64u)

#define MAGIC0  'K'
#define MAGIC1  'M'
#define VERSION 1u

#define NO_SLOT 0xFFu

_Static_assert(LAYERS >= 1 && LAYERS <= 8, "the layer engine addresses at most 8 layers");
_Static_assert((uint32_t)HEADER + PAYLOAD <= SLOT_BYTES, "the keymap does not fit in a slot: raise keymap_sectors");

static __xdata uint16_t shadow[LAYERS][MATRIX_ROWS][MATRIX_COLS];
static __xdata uint8_t  chunk[CHUNK];

static bool     dirty;
static uint8_t  active_slot = NO_SLOT;
static uint16_t active_seq;

// the header just read by slot_header(); statics rather than out-parameters, as the
// firmware keeps its locals on a small stack
static uint16_t header_seq;
static uint16_t header_crc;

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

static __xdata uint16_t *word_at(uint16_t index)
{
    return &shadow[0][0][0] + index;
}

static void load_defaults(void)
{
    for (uint8_t layer = 0; layer < LAYERS; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                shadow[layer][row][col] = keymap_default(layer, row, col);
            }
        }
    }
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
    flash_read_into(FLASH_CODE, slot_addr(slot), chunk, 10);

    if (chunk[0] != MAGIC0 || chunk[1] != MAGIC1 || chunk[2] != VERSION || chunk[3] != LAYERS || chunk[4] != MATRIX_ROWS || chunk[5] != MATRIX_COLS) {
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
        load_defaults();
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

void keymap_store_reset(void)
{
    load_defaults();
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
    flash_program_from(FLASH_CODE, base, chunk, 10);

    active_slot = target;
    active_seq  = seq;
}

#endif // FLASH_KEYMAP_ADDR
