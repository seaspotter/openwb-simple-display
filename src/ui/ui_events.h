// Widget-event glue that doesn't belong inside a specific reusable widget.
#pragma once

#include <cstdint>

namespace ui {

// Starts the lv_timer that polls command results for toast/error feedback.
void InitEvents();

// Shows a short-lived toast label at the bottom of the active screen.
void ShowToast(const char* message, bool is_error);

// Coalesces rapid +/- stepper taps into one setChargecurrent call after ~400ms idle.
void RequestChargecurrentDebounced(uint8_t chargepoint_id, float amps);

} // namespace ui
