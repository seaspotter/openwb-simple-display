// Shared model between the net task (writer) and the UI task (reader).
#pragma once

#include <cstdint>

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#endif

#include "openwb/openwb_models.h"

namespace state {

#ifdef UNIT_TEST
using PortMux = int; // no FreeRTOS on host builds; the lock degrades to a no-op
#else
using PortMux = portMUX_TYPE;
#endif

// Result of the most recent touch-triggered command, polled by the UI via a queue.
struct CommandResult {
    uint8_t chargepoint_id = 0;
    bool success = false;
    char message[96] = "";
};

class AppState {
public:
    void SetChargepointStatus(uint8_t index, const openwb::ChargepointStatus& status);
    openwb::ChargepointStatus GetChargepointStatus(uint8_t index) const;

    void SetGlobalStatus(const openwb::GlobalStatus& status);
    openwb::GlobalStatus GetGlobalStatus() const;

    void NoteTouch(uint32_t now_ms);
    uint32_t LastTouchMillis() const;

    // Gates automatic OTA installs.
    bool AnyChargepointCharging() const;

    void SetMqttConnected(bool connected);
    bool IsMqttConnected() const;

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
    bool mqtt_connected_ = false;
};

// Process-wide instance, shared by the net task and the UI task.
AppState& State();

} // namespace state
