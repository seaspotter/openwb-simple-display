// GT911 touch is initialized together with the display by board::InitDisplay()
// (esp32-smartdisplay registers it as LVGL's default input device internally) — this
// module exists for the small amount of touch-specific behavior that library doesn't
// cover: rotation-aware coordinate handling when AppConfig::rotation != 0, and exposing
// the shared "last touch" timestamp used by screen-timeout and OTA idle-gating (plan §6).
#pragma once

#include <cstdint>

namespace board {

// Call once after InitDisplay(). Applies AppConfig::rotation to the registered input
// device so touch coordinates match the rotated display output.
void SetTouchRotation(uint16_t rotation_degrees);

// Milliseconds since boot of the last touch event, as observed via an LVGL input-device
// read callback hook. Used by state::AppState to drive screen-timeout dimming and OTA
// idle-gating without duplicating touch-tracking logic in multiple places.
uint32_t LastTouchMillis();

} // namespace board
