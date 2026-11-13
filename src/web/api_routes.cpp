#include "api_routes.h"

#include <algorithm>

#include <ArduinoJson.h>
#include <AsyncJson.h>

#include "app_config_build.h"
#include "board/display_driver.h"
#include "config/config_store.h"
#include "net/wifi_manager.h"
#include "ota/ota_manager.h"
#include "state/app_state.h"

namespace web {
namespace {

constexpr const char* kMasked = "********";

void WriteChargepointJson(JsonObject obj, const openwb::ChargepointStatus& cp) {
    obj["id"] = cp.id;
    obj["name"] = cp.name;
    obj["valid"] = cp.valid;
    obj["power_w"] = cp.power_w;
    obj["plugged"] = cp.plugged;
    obj["charging"] = cp.charging;
    obj["state_str"] = cp.state_str;
    obj["fault_str"] = cp.fault_str;
    obj["chargemode"] = cp.chargemode;
    obj["soc_percent"] = cp.soc_percent;
    obj["connected_vehicle_name"] = cp.connected_vehicle_name;
    obj["phases_in_use"] = cp.phases_in_use;
    obj["evse_current_a"] = cp.evse_current_a;
    obj["energy_charged_wh"] = cp.energy_charged_wh;
    obj["locked"] = cp.locked;
    obj["last_update_ms"] = cp.last_update_ms;
}

void HandleGetStatus(AsyncWebServerRequest* request) {
    const config::AppConfig& cfg = config::Store().Get();

    JsonDocument doc;
    JsonArray cps = doc["chargepoints"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.chargepoint_count && i < config::AppConfig::kMaxChargepoints; ++i) {
        WriteChargepointJson(cps.add<JsonObject>(), state::State().GetChargepointStatus(i));
    }

    openwb::GlobalStatus global = state::State().GetGlobalStatus();
    JsonObject g = doc["global"].to<JsonObject>();
    g["valid"] = global.valid;
    g["pv_power_w"] = global.pv_power_w;
    g["grid_power_w"] = global.grid_power_w;
    g["battery_power_w"] = global.battery_power_w;
    g["battery_soc_percent"] = global.battery_soc_percent;
    g["consumer_power_w"] = global.consumer_power_w;

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void HandleGetConfig(AsyncWebServerRequest* request) {
    const config::AppConfig& cfg = config::Store().Get();

    JsonDocument doc;
    doc["device_name"] = cfg.device_name;
    doc["mqtt_host"] = cfg.mqtt_host;
    doc["mqtt_port"] = cfg.mqtt_port;
    doc["mqtt_username"] = cfg.mqtt_username;
    doc["has_mqtt_password"] = cfg.mqtt_password[0] != '\0';
    doc["mqtt_password"] = cfg.mqtt_password[0] != '\0' ? kMasked : "";
    doc["mqtt_use_tls"] = cfg.mqtt_use_tls;
    doc["mqtt_validate_cert"] = cfg.mqtt_validate_cert;
    doc["chargepoint_count"] = cfg.chargepoint_count;
    JsonArray ids = doc["chargepoint_ids"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.chargepoint_count; ++i) ids.add(cfg.chargepoint_ids[i]);
    doc["brightness_pct"] = cfg.brightness_pct;
    doc["screen_timeout_s"] = cfg.screen_timeout_s;
    doc["rotation"] = cfg.rotation;
    doc["dark_mode"] = cfg.dark_mode;
    doc["ota_auto_check"] = cfg.ota_auto_check;
    doc["ota_manifest_url"] = cfg.ota_manifest_url;

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void HandlePostConfig(AsyncWebServerRequest* /*request*/, JsonVariant& json) {
    JsonObject body = json.as<JsonObject>();

    config::Store().Update([&](config::AppConfig& c) {
        if (body["mqtt_host"].is<const char*>())
            strlcpy(c.mqtt_host, body["mqtt_host"], sizeof(c.mqtt_host));
        if (body["mqtt_port"].is<int>()) c.mqtt_port = body["mqtt_port"];
        if (body["mqtt_username"].is<const char*>())
            strlcpy(c.mqtt_username, body["mqtt_username"], sizeof(c.mqtt_username));
        // Only overwrite a secret if a real (non-masked, non-empty) value was sent —
        // the GET handler never echoes the real secret back, so the web UI's "leave
        // unchanged" affordance is simply "don't include this field in the PATCH body".
        if (body["mqtt_password"].is<const char*>() && strlen(body["mqtt_password"]) > 0)
            strlcpy(c.mqtt_password, body["mqtt_password"], sizeof(c.mqtt_password));
        if (body["mqtt_use_tls"].is<bool>()) c.mqtt_use_tls = body["mqtt_use_tls"];
        if (body["mqtt_validate_cert"].is<bool>()) c.mqtt_validate_cert = body["mqtt_validate_cert"];
        if (body["brightness_pct"].is<int>()) c.brightness_pct = body["brightness_pct"];
        if (body["screen_timeout_s"].is<int>()) c.screen_timeout_s = body["screen_timeout_s"];
        if (body["rotation"].is<int>()) c.rotation = body["rotation"];
        if (body["dark_mode"].is<bool>()) c.dark_mode = body["dark_mode"];
        if (body["ota_auto_check"].is<bool>()) c.ota_auto_check = body["ota_auto_check"];
        if (body["ota_manifest_url"].is<const char*>())
            strlcpy(c.ota_manifest_url, body["ota_manifest_url"], sizeof(c.ota_manifest_url));
        if (body["chargepoint_ids"].is<JsonArray>()) {
            JsonArray ids = body["chargepoint_ids"];
            uint8_t count = std::min<size_t>(ids.size(), config::AppConfig::kMaxChargepoints);
            for (uint8_t i = 0; i < count; ++i) c.chargepoint_ids[i] = ids[i];
            c.chargepoint_count = count;
        }
    });

    // Applies live rather than only on next boot — there's no on-device brightness
    // slider anymore to do this (settings are web-UI-only; see ui/wifi_reset_prompt.h),
    // so this is the only place a brightness change actually takes effect.
    if (body["brightness_pct"].is<int>()) {
        board::SetBacklight(static_cast<uint8_t>(static_cast<int>(body["brightness_pct"])));
    }
    // Note: this handler doesn't send a response itself — AsyncCallbackJsonWebHandler's
    // onRequest below sends it after this body callback runs.
}

void HandleWifiReset(AsyncWebServerRequest* request) {
    // Responds before actually resetting — ResetAndReprovision() reboots the device, so
    // there'd be no connection left to send a response over afterward.
    request->send(200, "application/json", "{\"success\":true}");
    net::ResetAndReprovision();
}

void HandleGetInfo(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["project_name"] = PROJECT_NAME;
    doc["firmware_version"] = FW_VERSION;
    doc["uptime_s"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["chip_model"] = ESP.getChipModel();

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void HandleOtaCheck(AsyncWebServerRequest* request) {
    ota::UpdateCheckResult result = ota::CheckForUpdate(/*auto_triggered=*/false);

    JsonDocument doc;
    doc["success"] = result.success;
    doc["update_available"] = result.update_available;
    doc["current_version"] = result.current_version;
    doc["latest_version"] = result.latest_version;
    doc["message"] = result.message;

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void HandleOtaStart(AsyncWebServerRequest* request) {
    bool force = request->hasParam("force", true) && request->getParam("force", true)->value() == "1";
    ota::InstallResult result = ota::StartInstall(force);

    JsonDocument doc;
    doc["started"] = result.started;
    doc["reason"] = result.reason;

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void HandleOtaProgress(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["progress_percent"] = ota::InstallProgressPercent();

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

} // namespace

void RegisterApiRoutes(AsyncWebServer& server) {
    server.on("/api/status", HTTP_GET, HandleGetStatus);
    server.on("/api/config", HTTP_GET, HandleGetConfig);
    server.on("/api/info", HTTP_GET, HandleGetInfo);
    server.on("/api/ota/check", HTTP_POST, HandleOtaCheck);
    server.on("/api/ota/start", HTTP_POST, HandleOtaStart);
    server.on("/api/ota/progress", HTTP_GET, HandleOtaProgress);
    server.on("/api/wifi/reset", HTTP_POST, HandleWifiReset);

    auto* config_post_handler = new AsyncCallbackJsonWebHandler(
        "/api/config",
        [](AsyncWebServerRequest* request, JsonVariant json) {
            HandlePostConfig(request, json);
            request->send(200, "application/json", "{\"success\":true}");
        });
    server.addHandler(config_post_handler);
}

} // namespace web
