#include "report.h"
#include "usb.h"
#include "kbdef.h"
#ifdef HUD_ENABLE
#    include "hud.h"
#endif

void kb_send_report(__xdata report_keyboard_t *report)
{
    usb_send_report(report);
}

void kb_send_nkro(__xdata report_nkro_t *report)
{
    usb_send_nkro(report);
}

void kb_send_extra(__xdata report_extra_t *report)
{
    usb_send_extra(report);
}

#ifdef HUD_ENABLE
// The HUD_TOG key is not sent to the host: the overlay sees a counter change.
bool kb_process_record(uint16_t keycode, bool key_pressed)
{
    if (keycode == HUD_TOGGLE_KEYCODE) {
        if (key_pressed) {
            hud_toggle_pressed();
        }
        return false;
    }

    return true;
}
#endif
