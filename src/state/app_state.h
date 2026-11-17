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

    // Drives the UI task's boot -> provisioning/status screen transition (see
    // lvgl_port.cpp); written by the net task, read by the UI task.
    void SetProvisioningActive(bool active);
    bool IsProvisioningActive() const;
    void SetNetworkReady(bool ready);
    bool IsNetworkReady() const;

    // NVS writes block for long enough to visibly glitch the display if done on the UI
    // task, which shares a core with rendering. The on-device settings screen sets this
    // instead of writing to config::Store() directly; the net task (core 0, not involved
    // in rendering) picks it up and does the actual write.
    void SetPendingBrightnessSave(uint8_t percent);
    bool TryTakePendingBrightnessSave(uint8_t* out_percent);

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
    bool provisioning_active_ = false;
    bool network_ready_ = false;
    bool brightness_save_pending_ = false;
    uint8_t pending_brightness_percent_ = 0;
};

// Process-wide instance, shared by the net task and the UI task.
AppState& State();

} // namespace state
