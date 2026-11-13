// Segmented control for Sofort/PV/Ziel/Eco/Stopp. Tapping a segment enqueues a
// setChargemode command and optimistically highlights the new selection.
#pragma once

#include <lvgl.h>

#include "openwb/openwb_models.h"

namespace widgets {

lv_obj_t* CreateModeSwitch(lv_obj_t* parent, uint8_t chargepoint_id);

// Call whenever a status update reports a chargemode, so the control reflects reality.
void SetSelectedMode(lv_obj_t* mode_switch, openwb::ChargeMode mode);

} // namespace widgets
