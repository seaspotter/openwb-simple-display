#include "net_task.h"

#include <Arduino.h>
#include <WiFi.h>

#include "config/config_store.h"
#include "ota/ota_gate.h"
#include "ota/ota_manager.h"
#include "openwb/openwb_mqtt.h"
#include "time_sync.h"
#include "web/web_server.h"
#include "wifi_manager.h"

namespace net {
namespace {

QueueHandle_t g_command_queue = nullptr;
QueueHandle_t g_result_queue = nullptr;
TaskHandle_t g_task_handle = nullptr;

// MQTT writes are fire-and-forget (plan §5) — no per-command success/message envelope
// like the retired HTTP client had. The one failure this can actually detect locally is
// "not connected to the broker at all", which is still worth surfacing as a toast rather
// than silently dropping the touch action.
state::CommandResult ExecuteCommand(const CommandRequest& req) {
    state::CommandResult result;
    result.chargepoint_id = req.chargepoint_id;

    if (!openwb::mqtt::IsConnected()) {
        result.success = false;
        strlcpy(result.message, "MQTT nicht verbunden", sizeof(result.message));
        return result;
    }

    switch (req.type) {
        case CommandType::kSetChargemode:
            openwb::mqtt::SetChargemode(req.chargepoint_id, req.mode);
            break;
        case CommandType::kSetChargecurrent:
            openwb::mqtt::SetChargecurrent(req.chargepoint_id, req.amps);
            break;
        case CommandType::kSetChargepointLock:
            openwb::mqtt::SetChargepointLock(req.chargepoint_id, req.locked);
            break;
    }
    result.success = true;
    return result;
}

void NetTask(void* /*param*/) {
    net::Provision(); // blocks here on first boot until the captive portal is completed;
                       // returns promptly on subsequent boots once WiFi connects.

    time_sync::Init();
    web::InitServer();
    openwb::mqtt::Init();

    uint32_t last_ota_check_ms = 0;
    bool app_marked_valid = false;

    for (;;) {
        const config::AppConfig& cfg = config::Store().Get();
        uint32_t now = millis();

        openwb::mqtt::Loop(); // non-blocking; pumps socket I/O and handles reconnect —
                               // push-based, so there's no poll interval to gate on here
                               // the way the retired HTTP poller needed.

        // First successful MQTT connection confirms this boot is healthy — cancels the
        // bootloader's pending-rollback state (plan §8) so an OTA update isn't
        // automatically reverted on the next unrelated crash/reboot.
        if (!app_marked_valid && openwb::mqtt::IsConnected()) {
            ota::MarkAppValidAfterSuccessfulBoot();
            app_marked_valid = true;
        }

        if (cfg.ota_auto_check && now - last_ota_check_ms >= cfg.ota_check_interval_s * 1000UL) {
            if (ota::ShouldAttemptAutomaticInstall()) {
                ota::CheckForUpdate(/*auto_triggered=*/true);
            }
            last_ota_check_ms = now;
        }

        CommandRequest req;
        if (xQueueReceive(g_command_queue, &req, 0) == pdTRUE) {
            state::CommandResult result = ExecuteCommand(req);
            xQueueSend(g_result_queue, &result, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

} // namespace

void Init() {
    g_command_queue = xQueueCreate(8, sizeof(CommandRequest));
    g_result_queue = xQueueCreate(8, sizeof(state::CommandResult));

    // Core 0, leaving core 1 dedicated to LVGL (plan §5).
    xTaskCreatePinnedToCore(NetTask, "net", 8192, nullptr, 1, &g_task_handle, 0);
}

bool EnqueueCommand(const CommandRequest& request) {
    if (g_command_queue == nullptr) return false;
    return xQueueSend(g_command_queue, &request, 0) == pdTRUE;
}

bool TryGetCommandResult(state::CommandResult& out_result) {
    if (g_result_queue == nullptr) return false;
    return xQueueReceive(g_result_queue, &out_result, 0) == pdTRUE;
}

} // namespace net
