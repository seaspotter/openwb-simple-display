// ST7701S 480x480 panel bring-up and LVGL display registration.
#pragma once

#include <lvgl.h>

namespace board {

// Initializes the ST7701S panel (3-wire SPI init sequence + RGB/DPI data interface),
// allocates its frame buffer(s), and registers an LVGL display driver backed by them.
// Must be called after lv_init(). Returns the created display, or nullptr on failure.
lv_display_t* InitDisplay();

// Backlight brightness, 0-100. Safe to call at any time after InitDisplay().
void SetBacklight(uint8_t percent);

} // namespace board
