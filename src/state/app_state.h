// Shared model between the net task (writer) and the UI task (reader) — plan §5. Decouples
// polling/HTTP work from rendering: the net task never touches LVGL, the UI task never
// blocks on HTTP. Also centralizes the "last touch" timestamp so screen-timeout dimming
// and OTA idle-gating read one signal instead of duplicating touch-tracking.
#pragma once

#include <cstdint>

// Portable across real hardware and host builds (native unit tests, the LVGL desktop
// simulator under src/sim/) — UNIT_TEST is defined by both of those, neither of which
// has FreeRTOS, so the lock degrades to a no-op there (both are single-threaded).
#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#endif

#include "openwb/openwb_models.h"

namespace state {

#ifdef UNIT_TEST
using PortMux = int;
#else
using PortMux = portMUX_TYPE;
#endif

// Result of the most recent attempt to execute a touch-triggered command (plan §5's
// "net task executes async, result flows back through a queue the UI polls"). The UI
// polls this via a FreeRTOS queue rather than a shared mutable field, since a command
// result is a one-shot event, not a continuously-updated value.
struct CommandResult {
    uint8_t chargepoint_id = 0;
    bool success = false;
    char message[96] = "";
};

class AppState {
public:
    // --- Written by openwb::mqtt (net task), read by UI screens ---
    void SetChargepointStatus(uint8_t index, const openwb::ChargepointStatus& status);
    openwb::ChargepointStatus GetChargepointStatus(uint8_t index) const;

    void SetGlobalStatus(const openwb::GlobalStatus& status);
    openwb::GlobalStatus GetGlobalStatus() const;

    // --- Written by board::touch_driver (via the LVGL task), read by net_task/ota_gate ---
    void NoteTouch(uint32_t now_ms);
    uint32_t LastTouchMillis() const;

    // --- Written by openwb::mqtt, read by ota_gate: is any configured chargepoint
    // currently charging? Automatic OTA installs are gated on this being false. ---
    bool AnyChargepointCharging() const;

private:
#ifdef UNIT_TEST
    mutable PortMux lock_ = 0;
#else
    mutable PortMux lock_ = portMUX_INITIALIZER_UNLOCKED;
#endif

    static constexpr uint8_t kMaxChargepoints = 4; // matches AppConfig::kMaxChargepoints
    openwb::ChargepointStatus chargepoints_[kMaxChargepoints]{};
    openwb::GlobalStatus global_{};
    uint32_t last_touch_ms_ = 0;
};

// Process-wide instance — see config::Store() for the same "explicit shared singleton"
// rationale; both the net task and the UI task need the identical instance.
AppState& State();

} // namespace state
