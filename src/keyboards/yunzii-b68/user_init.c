#include "kbdef.h"
#include "user_init.h"
#include "gpio.h"

// Reproduces the idle GPIO state the stock firmware sets up (function at 0xA627
// in the dump, see docs/keyboards/yunzii-b68-firmware-stock.md). Pins whose
// function we do not know yet (LEDs, radio, switches) are left exactly as the
// stock firmware leaves them, in the same register order.
void user_gpio_init();

void user_init()
{
    user_gpio_init();
}

void user_gpio_init()
{
    // 1 = output
    GPIO_DIR_WRITE(0, 0x04);
    GPIO_DIR_WRITE(1, 0x3F);
    GPIO_DIR_WRITE(2, 0x3F);
    GPIO_DIR_WRITE(3, 0x3F);
    GPIO_DIR_WRITE(4, 0x2F);
    GPIO_DIR_WRITE(5, 0x87);
    GPIO_DIR_WRITE(6, 0xFF);
    GPIO_DIR_WRITE(7, 0x70);

    // 1 = pull-up on
    GPIO_PULLUP_WRITE(0, 0xF8);
    GPIO_PULLUP_WRITE(1, 0x3F);
    GPIO_PULLUP_WRITE(2, 0x3F);
    GPIO_PULLUP_WRITE(3, 0x3F);
    GPIO_PULLUP_WRITE(4, 0x6F);
    GPIO_PULLUP_WRITE(5, 0x9F);
    GPIO_PULLUP_WRITE(6, 0xFF);
    GPIO_PULLUP_WRITE(7, 0xFF);

    // Output latches: column strobes (P4, P5, P6) idle high, P1-P3 low.
    GPIO_WRITE(0, 0x04);
    GPIO_WRITE(1, 0x00);
    GPIO_WRITE(2, 0x00);
    GPIO_WRITE(3, 0x00);
    GPIO_WRITE(4, 0x2D);
    GPIO_WRITE(5, 0x87);
    GPIO_WRITE(6, 0xFF);
    GPIO_WRITE(7, 0x00);

    // Drive strength of P1, P2, P3, P5: the stock firmware writes 0, which is
    // GPIO_DRIVE_25MA (same value the Air60 port uses).
    DRVCON = DRVCON_UNLOCK_P1;
    P1DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_UNLOCK_P2;
    P2DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_UNLOCK_P3;
    P3DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_UNLOCK_P5;
    P5DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_LOCK;
}
