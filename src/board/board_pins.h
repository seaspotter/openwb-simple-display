// Reference pin map for the Guition ESP32-4848S040C (ESP32-S3, 480x480 ST7701S RGB/DPI
// panel, GT911 capacitive touch over I2C).
//
// NOT consumed by display_driver.cpp/touch_driver.cpp directly: board bring-up uses the
// rzeldent/esp32-smartdisplay library, which gets this same pin map from the PlatformIO
// board definition (board id `esp32-4848S040CIY1`, see boards/ and platformio.ini) rather
// than from this file. This header exists purely as human-readable documentation of what
// that board definition encodes, kept here for anyone debugging at the hardware level.
//
// Confirmed directly against the vendored board definition at
// boards/esp32-4848S040CIY1.json (added as a git submodule from
// rzeldent/platformio-espressif32-sunton — see platformio.ini), and cross-checked
// against tobymarks/evcc-garage-display's src/board.h (same physical board, different
// project), which agrees on every value below.
#pragma once

#include <cstdint>

namespace board {

// --- RGB/DPI parallel LCD interface (16-bit RGB565) ---
inline constexpr int kLcdDataPins[16] = {
    4, 5, 6, 7, 15,       // B0-B4
    8, 20, 3, 46, 9, 10,  // G0-G5
    11, 12, 13, 14, 0,    // R0-R4
};
inline constexpr int kLcdHsyncPin = 16;
inline constexpr int kLcdVsyncPin = 17;
inline constexpr int kLcdDePin = 18;
inline constexpr int kLcdPclkPin = 21;
inline constexpr int kLcdBacklightPin = 38; // PWM-capable

inline constexpr int kLcdSpiCsPin = 39;
inline constexpr int kLcdSpiSckPin = 48;
inline constexpr int kLcdSpiMosiPin = 47;

inline constexpr int kLcdWidth = 480;
inline constexpr int kLcdHeight = 480;
// Confirmed from boards/esp32-4848S040CIY1.json's ST7701_PANEL_CONFIG_TIMINGS_PCLK_HZ.
inline constexpr uint32_t kLcdPclkHz = 12500000;

inline constexpr int kLcdHsyncPulseWidth = 8;
inline constexpr int kLcdHsyncBackPorch = 50;
inline constexpr int kLcdHsyncFrontPorch = 10;
inline constexpr int kLcdVsyncPulseWidth = 8;
inline constexpr int kLcdVsyncBackPorch = 20;
inline constexpr int kLcdVsyncFrontPorch = 10;

// --- GT911 capacitive touch (I2C) ---
inline constexpr int kTouchSdaPin = 19;
inline constexpr int kTouchSclPin = 45;
inline constexpr uint8_t kTouchI2cAddress = 0x5D;
inline constexpr int kTouchIntPin = -1; // GT911_TOUCH_CONFIG_INT=GPIO_NUM_NC, confirmed
inline constexpr int kTouchRstPin = -1; // GT911_TOUCH_CONFIG_RST=GPIO_NUM_NC, confirmed

// --- Gotchas ---
// GPIO0 doubles as R4 and the BOOT strapping pin: must not be pulled low at reset.
// GPIO3/45/46 are ESP32-S3 strapping pins (G2, touch SCL, G3): pull state at reset matters.
// GPIO19/20 are the native-USB D-/D+ pins, reused here for touch SDA / LCD G1 — native USB
// CDC must stay disabled (sdkconfig.defaults: CONFIG_ARDUINO_USB_CDC_ON_BOOT=n).
// The board JSON also defines BOARD_HAS_LED/LED_GPIO=16 — this is a real conflict with
// HSYNC (also GPIO16), confirmed in the vendored JSON itself, not just a suspected
// copy-paste error. Don't use GPIO16 for anything but HSYNC on this board.

} // namespace board
