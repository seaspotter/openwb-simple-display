// Dedicated FreeRTOS task (core 0) owning everything network-related: WiFi provisioning,
// the MQTT client loop, the web settings server, and OTA checks (plan §5). The UI task
// (core 1) never touches any of this directly — it only reads state::AppState and
// enqueues command requests, so an in-flight MQTT publish never blocks rendering/touch.
#pragma once

#include <cstdint>

#include "openwb/openwb_models.h"
#include "state/app_state.h"

namespace net {

// Starts the net task. Called once from setup(), after ui::Init() (see main.cpp).
void Init();

// A touch-triggered control action, queued by ui_events.cpp and executed on the net
// task so the HTTP POST never runs on (or blocks) the UI task.
enum class CommandType { kSetChargemode, kSetChargecurrent, kSetChargepointLock };

struct CommandRequest {
    CommandType type;
    uint8_t chargepoint_id;
    openwb::ChargeMode mode;      // used by kSetChargemode
    float amps;                   // used by kSetChargecurrent
    bool locked;                  // used by kSetChargepointLock
};

// Non-blocking; returns false if the internal queue is full (shouldn't happen in
// practice — it only ever holds a handful of in-flight touch actions).
bool EnqueueCommand(const CommandRequest& request);

// Non-blocking poll for a completed command's result, called from the LVGL task via an
// lv_timer (plan §5). Returns false if nothing has completed since the last call.
bool TryGetCommandResult(state::CommandResult& out_result);

} // namespace net
