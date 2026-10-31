// Host unit tests for src/smk/keymap_store.c against a simulated flash that can lose
// power at any program/erase step and leaves the cell it was working on half-written.

#include "keymap_store.h"
#include "flash.h"
#include "kbdef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLASH_SIZE 0x2000u
#define SECTOR     FLASH_KEYMAP_SECTOR_SIZE

static uint8_t flash[FLASH_SIZE];
static long    cut_at = -1; // index of the modifying op the power goes during; -1 = never
static long    ops;         // modifying ops started so far

enum fate { FULL, PARTIAL, NOTHING };

// What happens to the next modifying op: it completes, it is the one interrupted, or the power is already gone.
static enum fate next_op(void)
{
    const long index = ops++;

    if (cut_at < 0 || index < cut_at) {
        return FULL;
    }
    return index == cut_at ? PARTIAL : NOTHING;
}

void flash_read_into(flash_window_t window, uint16_t addr, uint8_t *dst, uint8_t len)
{
    (void)window;
    memcpy(dst, &flash[addr], len);
}

bool flash_matches(flash_window_t window, uint16_t addr, const uint8_t *src, uint8_t len)
{
    (void)window;
    return memcmp(&flash[addr], src, len) == 0;
}

void flash_program_byte(flash_window_t window, uint16_t addr, uint8_t value)
{
    (void)window;
    switch (next_op()) {
        case FULL:
            flash[addr] &= value; // programming only ever clears bits
            break;
        case PARTIAL: // some of the bits to clear did not make it
            flash[addr] &= (uint8_t)(value | (uint8_t)rand());
            break;
        case NOTHING:
            break;
    }
}

void flash_program_from(flash_window_t window, uint16_t addr, const uint8_t *src, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        flash_program_byte(window, (uint16_t)(addr + i), src[i]);
    }
}

void flash_erase(flash_window_t window, uint16_t addr)
{
    (void)window;
    const uint16_t base = (uint16_t)(addr & ~(SECTOR - 1u));

    switch (next_op()) {
        case FULL:
            memset(&flash[base], 0xFF, SECTOR);
            break;
        case PARTIAL: // some cells erased, the rest still hold the old data
            for (uint16_t i = 0; i < SECTOR; i++) {
                if (rand() & 1) {
                    flash[base + i] = 0xFF;
                }
            }
            break;
        case NOTHING:
            break;
    }
}

uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    return (uint16_t)(0x100u + layer * 16u + row * 4u + col);
}

static int failures;
static int checks;

#define CHECK(cond)                                                \
    do {                                                           \
        checks++;                                                  \
        if (!(cond)) {                                             \
            failures++;                                            \
            printf("FAIL line %d: %s\n", __LINE__, #cond);         \
        }                                                          \
    } while (0)

static void power_on_fresh(void)
{
    memset(flash, 0xFF, sizeof flash);
    cut_at = -1;
    ops    = 0;
    keymap_store_init();
}

// The keymap that edit number `n` produces: position (0,0,0) and (7,1,2) carry n.
static void apply_edit(uint16_t n)
{
    keymap_store_set(0, 0, 0, (uint16_t)(0x1000u + n));
    keymap_store_set(7, 1, 2, (uint16_t)(0x2000u + n));
    keymap_store_set(3, 0, 1, (uint16_t)(0x3000u + n));
}

static int is_edit(uint16_t n)
{
    return keymap_store_get(0, 0, 0) == (uint16_t)(0x1000u + n) && keymap_store_get(7, 1, 2) == (uint16_t)(0x2000u + n) && keymap_store_get(3, 0, 1) == (uint16_t)(0x3000u + n);
}

static int is_default(void)
{
    for (uint8_t l = 0; l < 8; l++) {
        for (uint8_t r = 0; r < 2; r++) {
            for (uint8_t c = 0; c < 3; c++) {
                if (keymap_store_get(l, r, c) != keymap_default(l, r, c)) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

static void reboot(void)
{
    cut_at = -1; // power is back
    keymap_store_init();
}

static void test_fresh_flash_gives_defaults(void)
{
    power_on_fresh();
    CHECK(is_default());
    CHECK(!keymap_store_dirty());
    keymap_store_flush(); // nothing to write
    CHECK(ops == 0);
}

static void test_edit_survives_reboot(void)
{
    power_on_fresh();
    CHECK(keymap_store_set(2, 1, 1, 0x1234));
    CHECK(keymap_store_dirty());
    keymap_store_flush();
    CHECK(!keymap_store_dirty());
    reboot();
    CHECK(keymap_store_get(2, 1, 1) == 0x1234);
    CHECK(keymap_store_get(2, 0, 0) == keymap_default(2, 0, 0)); // the rest untouched
}

static void test_same_value_is_not_an_edit(void)
{
    power_on_fresh();
    CHECK(keymap_store_set(0, 0, 0, keymap_default(0, 0, 0)));
    CHECK(!keymap_store_dirty());
}

static void test_out_of_range(void)
{
    power_on_fresh();
    CHECK(!keymap_store_set(8, 0, 0, 1));
    CHECK(!keymap_store_set(0, 2, 0, 1));
    CHECK(!keymap_store_set(0, 0, 3, 1));
    CHECK(!keymap_store_dirty());
}

static void test_reset_stores_defaults(void)
{
    power_on_fresh();
    apply_edit(1);
    keymap_store_flush();
    keymap_store_reset();
    CHECK(keymap_store_dirty());
    CHECK(is_default());
    keymap_store_flush();
    reboot();
    CHECK(is_default());
}

static void test_many_saves_alternate_and_wrap_the_sequence(void)
{
    power_on_fresh();
    for (uint32_t n = 1; n <= 70000; n++) { // past the 16-bit sequence wrap
        apply_edit((uint16_t)n);
        keymap_store_flush();
        if (n % 9973 == 0 || n == 65535 || n == 65536 || n == 65537 || n == 70000) {
            reboot();
            CHECK(is_edit((uint16_t)n));
        }
    }
}

// Cut the power at every single step of a save; the keymap after the reboot must be exactly
// the one before the save or exactly the one the save was writing, never a mix.
static void test_power_cut_at_every_step(void)
{
    for (int completed = 0; completed < 3; completed++) { // saves already done: 0 (blank), 1, 2
        long total = -1;                                  // ops in a full save, learned on the first pass

        for (long cut = -1; total < 0 || cut <= total; cut++) {
            power_on_fresh();
            for (int save = 1; save <= completed; save++) {
                apply_edit((uint16_t)save);
                keymap_store_flush();
            }

            apply_edit(100);
            ops    = 0;
            cut_at = cut; // -1 on the first pass: no cut, to count the ops
            keymap_store_flush();
            if (cut < 0) {
                total = ops;
            }
            reboot();

            const int old_ok = (completed == 0) ? is_default() : is_edit((uint16_t)completed);
            const int new_ok = is_edit(100);
            CHECK(old_ok || new_ok);
            if (cut < 0 || cut >= total) {
                CHECK(new_ok); // the power held for the whole save
            }
            if (cut >= 0 && cut < total - 10) {
                CHECK(old_ok); // before the header starts landing the old keymap is still the one in use
            }
        }
        CHECK(total > 100);
    }
}

static void flip(uint16_t addr)
{
    flash[addr] ^= 0x01;
}

static void test_corrupt_payload_falls_back(void)
{
    power_on_fresh();
    apply_edit(1);
    keymap_store_flush(); // slot A
    apply_edit(2);
    keymap_store_flush(); // slot B, newest

    flip(FLASH_KEYMAP_ADDR + 2 * SECTOR + 20); // a payload byte of slot B
    reboot();
    CHECK(is_edit(1)); // B fails its CRC: A is used

    flip(FLASH_KEYMAP_ADDR + 20); // and now A is broken as well
    reboot();
    CHECK(is_default());
}

static void test_other_shape_is_ignored(void)
{
    power_on_fresh();
    apply_edit(1);
    keymap_store_flush();
    flash[FLASH_KEYMAP_ADDR + 5] = 7; // header says another column count
    reboot();
    CHECK(is_default());
}

int main(void)
{
    srand(12345);

    test_fresh_flash_gives_defaults();
    test_edit_survives_reboot();
    test_same_value_is_not_an_edit();
    test_out_of_range();
    test_reset_stores_defaults();
    test_many_saves_alternate_and_wrap_the_sequence();
    test_power_cut_at_every_step();
    test_corrupt_payload_falls_back();
    test_other_shape_is_ignored();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
