#include "raw_hid.h"

#ifdef RAW_HID_ENABLE

#include "usbhw.h"
#include "usb.h"
#include "via.h"
#include "delay.h"
#ifdef ISP_ENABLE
#    include "isp.h"
#endif
#include <stdbool.h>

// One request is handled at a time. While `pending` is set EP2 OUT is not re-armed, so the host's
// next packet is NAKed and retried; nothing is queued, nothing is lost.
static __xdata uint8_t   packet[RAW_HID_SIZE];
static volatile bool     pending;

void raw_hid_reset(void)
{
    pending = false;
    usb_hw_ep2_out_arm();
}

void raw_hid_out_irq(void)
{
    if (pending) {
        return; // cannot happen while the endpoint is not armed; ignore a stray one
    }

    if (usb_hw_ep2_out_read(packet, RAW_HID_SIZE) == RAW_HID_SIZE) {
        pending = true; // answered from the main loop
    } else {
        usb_hw_ep2_out_arm(); // not a report of ours: take the next packet
    }
}

void raw_hid_task(void)
{
    if (!pending) {
        return;
    }

    via_receive(packet, RAW_HID_SIZE);

    if (usb_is_configured()) {
        usb_hw_ep2_in_send(packet, RAW_HID_SIZE);
    }

#ifdef ISP_ENABLE
    if (via_bootloader_requested()) {
        delay_ms(100); // the host reads the reply first
        isp_jump();
    }
#endif

    pending = false;
    usb_hw_ep2_out_arm();
}

#endif // RAW_HID_ENABLE
