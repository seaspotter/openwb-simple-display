// Per-chargepoint card: status, vehicle/SoC, mode switch, current stepper, lock toggle.
#pragma once

#include <cstdint>

#include <lvgl.h>

#include "openwb/openwb_models.h"

namespace screens {

lv_obj_t* CreateChargepointCard(lv_obj_t* parent, uint8_t chargepoint_id);
void RefreshChargepointCard(lv_obj_t* card, const openwb::ChargepointStatus& status);

// Screenshot/documentation tool only (see src/sim/screenshot_main.cpp) — opens the
// per-mode settings popup for `card` without needing a simulated touch event.
void DebugOpenSettingsPopup(lv_obj_t* card);

} // namespace screens
