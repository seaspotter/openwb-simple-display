// Reference pin map for the Guition ESP32-4848S040C. Not consumed directly by code —
// esp32-smartdisplay gets these from the PlatformIO board definition instead
// (boards/esp32-4848S040CIY1.json); this is documentation for hardware-level debugging.
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
// GPIO0 (R4) is also the BOOT strapping pin. GPIO3/45/46 are strapping pins too.
// GPIO19/20 are native-USB D-/D+, reused for touch SDA / LCD G1 — USB CDC must stay
// disabled (sdkconfig.defaults: CONFIG_ARDUINO_USB_CDC_ON_BOOT=n).
// GPIO16 is HSYNC; the board JSON's LED_GPIO=16 conflicts with it — don't reuse GPIO16.

} // namespace board
