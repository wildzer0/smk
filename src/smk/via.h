#pragma once

#include <stdint.h>
#include <stdbool.h>

// VIA / Vial command handling, as spoken by the Vial GUI over a 32-byte raw HID report.
//
// via_receive() takes one request and rewrites the same buffer into the reply, which the
// transport then sends back unchanged, exactly like QMK's raw_hid_receive(). Commands this
// firmware does not implement answer with 0xFF in the first byte ("unhandled").

#define VIA_RAW_SIZE 32

void via_receive(uint8_t *data, uint8_t length);

// set by the "bootloader jump" command (it needs the keyboard unlocked): the transport sends the
// reply first and then calls isp_jump() when this is true
bool via_bootloader_requested(void);
