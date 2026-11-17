// FreeRTOS task (core 0) owning WiFi, MQTT, the web server, and OTA checks — decoupled
// from the UI task (core 1) so network I/O never blocks rendering/touch.
#pragma once

#include <cstdint>

#include "openwb/openwb_models.h"
#include "state/app_state.h"

namespace net {

// Starts the net task. Call once from setup(), after ui::Init().
void Init();

// A touch-triggered control action, queued by ui_events.cpp and executed on the net task.
enum class CommandType {
    kSetChargemode,
    kSetChargecurrent,
    kSetChargepointLock,
    kSetInstantLimitType,
    kSetInstantLimitSoc,
    kSetInstantLimitAmount,
    kSetPvLimitType,
    kSetPvLimitSoc,
    kSetPvLimitAmount,
    kSetMaxPriceEco,
};

struct CommandRequest {
    CommandType type;
    uint8_t chargepoint_id;
    openwb::ChargeMode mode; // kSetChargemode
    float amps;              // kSetChargecurrent
    bool locked;             // kSetChargepointLock
    char limit_type[8];      // kSetInstantLimitType, kSetPvLimitType: "none"|"soc"|"amount"
    float value;             // kSet*LimitSoc/Amount (percent/kWh), kSetMaxPriceEco (ct)
};

// Non-blocking; false if the internal queue is full.
bool EnqueueCommand(const CommandRequest& request);

// Non-blocking poll for a completed command's result. False if none is ready.
bool TryGetCommandResult(state::CommandResult& out_result);

} // namespace net
