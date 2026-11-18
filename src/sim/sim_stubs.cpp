// Host-only stand-ins for the board::/net::/ota:: functions the UI calls, so
// sim_screenshot/sim_interactive can link without ESP-only dependencies.
#include <cstdio>

#include "board/display_driver.h"
#include "net/net_task.h"
#include "net/wifi_manager.h"
#include "ota/ota_manager.h"

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
    return false;
}

void ResetAndReprovision() {
    std::printf("[sim] ResetAndReprovision() called (no-op in simulator)\n");
}

} // namespace net

namespace ota {

bool IsUpdateAvailable() { return false; }

UpdateCheckResult CheckForUpdate(bool /*auto_triggered*/) {
    UpdateCheckResult result;
    result.success = true;
    snprintf(result.current_version, sizeof(result.current_version), "0.0.0-sim");
    snprintf(result.latest_version, sizeof(result.latest_version), "0.0.0-sim");
    result.update_available = false;
    std::printf("[sim] ota::CheckForUpdate() called (no-op in simulator)\n");
    return result;
}

InstallResult StartInstall(bool /*force*/) {
    InstallResult result;
    snprintf(result.reason, sizeof(result.reason), "nicht unterstützt im Simulator");
    std::printf("[sim] ota::StartInstall() called (no-op in simulator)\n");
    return result;
}

int InstallProgressPercent() { return -1; }

const char* InstallFailureReason() { return ""; }

} // namespace ota
