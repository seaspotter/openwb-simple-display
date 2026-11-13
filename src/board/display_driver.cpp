#include "display_driver.h"

#include <esp32_smartdisplay.h>

namespace board {

lv_display_t* InitDisplay() {
    // smartdisplay_init() brings up the ST7701S RGB/DPI panel and the GT911 touch
    // controller together (board identified at compile time via the PlatformIO board id,
    // see platformio.ini), and registers both with LVGL. It also sets backlight to 50%.
    smartdisplay_init();
    return lv_display_get_default();
}

void SetBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    smartdisplay_lcd_set_backlight(percent / 100.0f);
}

} // namespace board
