// Central home for widget-event -> command glue that doesn't belong inside a specific
// reusable widget (plan §6): polling net::TryGetCommandResult() for toast/error
// feedback, and debouncing the +/- current stepper so rapid taps coalesce into one
// setChargecurrent call instead of one per tap.
#pragma once

#include <cstdint>

namespace ui {

// Starts the lv_timer that polls command results. Call once from lvgl_port::Init(),
// after the first screen is shown.
void InitEvents();

// Shows a short-lived toast label at the bottom of the active screen.
void ShowToast(const char* message, bool is_error);

// Called by screen_chargepoint's +/- stepper on every tap; coalesces rapid taps into a
// single setChargecurrent call after ~400ms of inactivity (plan §6) rather than firing
// one HTTP POST per tap.
void RequestChargecurrentDebounced(uint8_t chargepoint_id, float amps);

} // namespace ui
