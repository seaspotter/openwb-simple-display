#include "net_task.h"

#include <Arduino.h>

#include "config/config_store.h"
#include "ota/ota_gate.h"
#include "ota/ota_manager.h"
#include "openwb/openwb_mqtt.h"
#include "state/app_state.h"
#include "time_sync.h"
#include "web/web_server.h"
#include "wifi_manager.h"

namespace net {
namespace {

QueueHandle_t g_command_queue = nullptr;
QueueHandle_t g_result_queue = nullptr;
TaskHandle_t g_task_handle = nullptr;

// MQTT writes are fire-and-forget; the only failure detectable here is no broker connection.
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
    net::Provision(); // blocks until provisioned on first boot
    state::State().SetNetworkReady(true); // tells the UI task to leave the boot screen

    time_sync::Init();
    web::InitServer();
    openwb::mqtt::Init();

    uint32_t last_ota_check_ms = 0;
    bool app_marked_valid = false;

    for (;;) {
        const config::AppConfig& cfg = config::Store().Get();
        uint32_t now = millis();

        openwb::mqtt::Loop();

        // The UI task can't call openwb::mqtt::IsConnected() directly (ESP-only).
        state::State().SetMqttConnected(openwb::mqtt::IsConnected());

        // Confirms the boot is healthy, cancelling the bootloader's pending rollback.
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

        uint8_t pending_brightness;
        if (state::State().TryTakePendingBrightnessSave(&pending_brightness)) {
            config::Store().Update(
                [pending_brightness](config::AppConfig& c) { c.brightness_pct = pending_brightness; });
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

} // namespace

void Init() {
    g_command_queue = xQueueCreate(8, sizeof(CommandRequest));
    g_result_queue = xQueueCreate(8, sizeof(state::CommandResult));

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
