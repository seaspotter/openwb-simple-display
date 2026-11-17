// Home screen: single chargepoint full-screen, or a swipeable pager with a persistent
// status bar + flow panel when multiple chargepoints are configured.
#pragma once

#include <cstdint>

#include <lvgl.h>

namespace screens {

void ShowStatusScreen(uint8_t chargepoint_count);

// Screenshot/documentation tool only (see src/sim/screenshot_main.cpp) — the first
// chargepoint card on the currently-shown status screen, or nullptr if none exists yet.
lv_obj_t* DebugGetFirstChargepointCard();

} // namespace screens
