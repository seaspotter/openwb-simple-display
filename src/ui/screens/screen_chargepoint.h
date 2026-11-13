// Per-chargepoint card (plan §6): plug/charge-state, power, vehicle/SoC, mode switch,
// +/- current stepper, lock toggle. One instance per configured chargepoint, built
// either full-screen (single chargepoint) or as a page inside the gesture_pager
// (multiple chargepoints) — screen_status.cpp decides which.
#pragma once

#include <cstdint>

#include <lvgl.h>

#include "openwb/openwb_models.h"

namespace screens {

// Builds the chargepoint card's widgets under `parent` (either a bare screen or one
// tileview page) and returns a handle screen_status.cpp stores so it can call Refresh()
// on each poll tick.
lv_obj_t* CreateChargepointCard(lv_obj_t* parent, uint8_t chargepoint_id);

// Updates the card's widgets from a freshly-polled status. Called from screen_status's
// lv_timer, not from the net task directly (plan §5: UI task reads state, never the
// reverse).
void RefreshChargepointCard(lv_obj_t* card, const openwb::ChargepointStatus& status);

} // namespace screens
