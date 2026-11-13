// Segmented control for instant/pv/eco/stop (plan §6). Tapping a segment enqueues a
// setChargemode command via net::EnqueueCommand and optimistically highlights the new
// selection; ui_events.cpp reconciles it against the next poll result.
#pragma once

#include <lvgl.h>

#include "openwb/openwb_models.h"

namespace widgets {

// Creates the control under `parent`, bound to `chargepoint_id`. Call
// SetSelectedMode() whenever a poll updates that chargepoint's reported chargemode, so
// the control reflects reality rather than only the last touch.
lv_obj_t* CreateModeSwitch(lv_obj_t* parent, uint8_t chargepoint_id);

void SetSelectedMode(lv_obj_t* mode_switch, openwb::ChargeMode mode);

} // namespace widgets
