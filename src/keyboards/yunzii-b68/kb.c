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

extern void indicators_next_effect();
extern void indicators_prev_effect();
extern void indicators_brightness_up();
extern void indicators_brightness_down();
extern void indicators_speed_up();
extern void indicators_speed_down();

// The board's custom keys are not sent to the host.
bool kb_process_record(uint16_t keycode, bool key_pressed)
{
    switch (keycode) {
#ifdef HUD_ENABLE
        case B68_HUD_TOG: // the overlay sees a counter change
            if (key_pressed) {
                hud_toggle_pressed();
            }
            return false;
#endif
        case B68_FX_PREV:
            if (key_pressed) {
                indicators_prev_effect();
            }
            return false;
        case B68_FX_NEXT:
            if (key_pressed) {
                indicators_next_effect();
            }
            return false;
        case B68_BRI_DN:
            if (key_pressed) {
                indicators_brightness_down();
            }
            return false;
        case B68_BRI_UP:
            if (key_pressed) {
                indicators_brightness_up();
            }
            return false;
        case B68_SPD_DN:
            if (key_pressed) {
                indicators_speed_down();
            }
            return false;
        case B68_SPD_UP:
            if (key_pressed) {
                indicators_speed_up();
            }
            return false;
        default:
            return true;
    }
}
