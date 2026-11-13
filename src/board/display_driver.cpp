#include "display_driver.h"

#include <esp32_smartdisplay.h>

namespace board {

lv_display_t* InitDisplay() {
    // Brings up the panel + GT911 touch and registers both with LVGL.
    smartdisplay_init();
    return lv_display_get_default();
}

void SetBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    smartdisplay_lcd_set_backlight(percent / 100.0f);
}

} // namespace board
