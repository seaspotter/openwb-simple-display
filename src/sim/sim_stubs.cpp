// Host-only stand-ins for the handful of board::/net:: functions the UI code calls
// directly (plan §5/§6: screens/widgets only ever touch state::AppState and these two
// modules' public functions, never WiFi/HTTP/display-driver internals directly) — lets
// the sim_screenshot/sim_interactive envs link against the real src/ui/* code without
// pulling in ESP-only dependencies (esp32-smartdisplay, WiFiManager, HTTPClient, ...).
// Excluded from the real ESP build via platformio.ini's build_src_filter.
#include <cstdio>

#include "board/display_driver.h"
#include "net/net_task.h"
#include "net/wifi_manager.h"

namespace board {

void SetBacklight(uint8_t percent) {
    std::printf("[sim] backlight -> %u%%\n", percent);
}

} // namespace board

namespace net {

bool EnqueueCommand(const CommandRequest& request) {
    const char* type_name = "?";
    switch (request.type) {
        case CommandType::kSetChargemode: type_name = "setChargemode"; break;
        case CommandType::kSetChargecurrent: type_name = "setChargecurrent"; break;
        case CommandType::kSetChargepointLock: type_name = "setChargepointLock"; break;
    }
    std::printf("[sim] command: %s cp=%u\n", type_name, request.chargepoint_id);
    return true;
}

bool TryGetCommandResult(state::CommandResult& /*out_result*/) {
    return false; // no pending results in the simulator — nothing async to resolve
}

void ResetAndReprovision() {
    std::printf("[sim] ResetAndReprovision() called (no-op in simulator)\n");
}

} // namespace net
