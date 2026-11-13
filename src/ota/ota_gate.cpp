#include "ota_gate.h"

#include <Arduino.h>

#include "state/app_state.h"

namespace ota {
namespace {
constexpr uint32_t kIdleThresholdMs = 5 * 60 * 1000;
} // namespace

bool ShouldAttemptAutomaticInstall() {
    if (state::State().AnyChargepointCharging()) return false;

    uint32_t last_touch = state::State().LastTouchMillis();
    if (last_touch != 0 && millis() - last_touch < kIdleThresholdMs) return false;

    return true;
}

} // namespace ota
