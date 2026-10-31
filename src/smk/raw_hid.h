#pragma once

#include <stdint.h>

// The raw HID interface VIA and Vial talk to: 32-byte reports in each direction over EP2.
// Only built into a board that defines RAW_HID_ENABLE.

#define RAW_HID_SIZE 32

// USB interrupt: a packet arrived on EP2 OUT. Copies it and leaves the endpoint busy (the host is
// told to retry) until the main loop has answered.
void raw_hid_out_irq(void);

// SET_CONFIGURATION: forget any half-handled packet and accept the first one.
void raw_hid_reset(void);

// Main loop: runs the request through VIA/Vial and sends the reply.
void raw_hid_task(void);
