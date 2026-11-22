#include "api_routes.h"

#include <algorithm>
#include <cctype>

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "app_config_build.h"
#include "board/display_driver.h"
#include "config/config_store.h"
#include "net/broker_discovery.h"
#include "net/wifi_manager.h"
#include "ota/ota_manager.h"
#include "state/app_state.h"
#include "util/logging.h"

namespace web {
namespace {

constexpr const char* kMasked = "********";
constexpr char kTag[] = "api_routes";

// Global, not per-IP: this device has one legitimate user at a time, so the simplicity
// of a single shared counter outweighs tracking individual source addresses. Not a
// hardened enterprise control, just enough to turn "guess PINs all day over WiFi" into
// a real cost instead of a free, instant, unlimited loop.
constexpr uint8_t kMaxFailedAuthAttempts = 5;
constexpr uint32_t kAuthLockoutMs = 5 * 60 * 1000;

uint8_t g_failed_auth_attempts = 0;
uint32_t g_auth_lockout_until_ms = 0;

// HTTP Basic Auth gate; disabled while AppConfig::web_ui_pin is empty.
bool CheckAuth(AsyncWebServerRequest* request) {
    const config::AppConfig& cfg = config::Store().Get();
    if (cfg.web_ui_pin[0] == '\0') return true;

    uint32_t now = millis();
    if (g_auth_lockout_until_ms != 0 && now < g_auth_lockout_until_ms) {
        request->send(429, "application/json",
                      "{\"error\":\"Zu viele Fehlversuche — bitte in ein paar Minuten erneut versuchen\"}");
        return false;
    }

    if (request->authenticate("user", cfg.web_ui_pin)) {
        g_failed_auth_attempts = 0;
        g_auth_lockout_until_ms = 0;
        return true;
    }

    // A browser's very first request to a protected endpoint has no Authorization
    // header at all yet (it hasn't been prompted for credentials), which would
    // otherwise count as a "failed attempt" on every single page load.
    if (request->hasHeader("Authorization")) {
        ++g_failed_auth_attempts;
        if (g_failed_auth_attempts >= kMaxFailedAuthAttempts) {
            g_auth_lockout_until_ms = now + kAuthLockoutMs;
            util::LogWarn(kTag, "too many failed web UI logins, locking out");
        }
    }

    // requestAuthentication()'s default is Digest auth, not Basic (despite its name) —
    // force Basic explicitly, matching what authenticate() checks above and what the
    // comment on this function always intended.
    request->requestAuthentication(/*realm=*/nullptr, /*isDigest=*/false);
    return false;
}

void SendJson(AsyncWebServerRequest* request, JsonDocument& doc) {
    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

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

void HandleGetDiscoveredChargepoints(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    openwb::DiscoveredChargepoint discovered[state::AppState::kMaxDiscoveredChargepoints];
    uint8_t count = state::State().GetDiscoveredChargepoints(discovered, state::AppState::kMaxDiscoveredChargepoints);

    JsonDocument doc;
    JsonArray arr = doc["chargepoints"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = discovered[i].id;
        obj["name"] = discovered[i].name;
    }
    SendJson(request, doc);
}

void HandleGetStatus(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
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
    g["home_consumption_w"] = global.home_consumption_w;
    g["meter_power_w"] = global.meter_power_w;

    SendJson(request, doc);
}

void HandleGetConfig(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    const config::AppConfig& cfg = config::Store().Get();

    JsonDocument doc;
    doc["device_name"] = cfg.device_name;
    doc["has_web_ui_pin"] = cfg.web_ui_pin[0] != '\0';
    doc["mqtt_host"] = cfg.mqtt_host;
    doc["mqtt_port"] = cfg.mqtt_port;
    doc["mqtt_username"] = cfg.mqtt_username;
    doc["has_mqtt_password"] = cfg.mqtt_password[0] != '\0';
    doc["mqtt_password"] = cfg.mqtt_password[0] != '\0' ? kMasked : "";
    doc["mqtt_use_tls"] = cfg.mqtt_use_tls;
    doc["mqtt_validate_cert"] = cfg.mqtt_validate_cert;
    doc["chargepoint_count"] = cfg.chargepoint_count;
    JsonArray ids = doc["chargepoint_ids"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.chargepoint_count && i < config::AppConfig::kMaxChargepoints; ++i)
        ids.add(cfg.chargepoint_ids[i]);
    doc["brightness_pct"] = cfg.brightness_pct;
    doc["screen_timeout_s"] = cfg.screen_timeout_s;
    doc["rotation"] = cfg.rotation;
    doc["dark_mode"] = cfg.dark_mode;
    doc["dark_mode_schedule"] = cfg.dark_mode_schedule;
    doc["schedule_dark_start_hour"] = cfg.schedule_dark_start_hour;
    doc["schedule_dark_start_min"] = cfg.schedule_dark_start_min;
    doc["schedule_light_start_hour"] = cfg.schedule_light_start_hour;
    doc["schedule_light_start_min"] = cfg.schedule_light_start_min;
    doc["show_flow_chart"] = cfg.show_flow_chart;
    doc["ota_auto_check"] = cfg.ota_auto_check;
    doc["ota_auto_install"] = cfg.ota_auto_install;
    doc["ota_manifest_url"] = cfg.ota_manifest_url;

    SendJson(request, doc);
}

bool HandlePostConfig(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!CheckAuth(request)) return false;
    JsonObject body = json.as<JsonObject>();
    if (body.isNull()) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"invalid body\"}");
        return false;
    }

    config::Store().Update([&](config::AppConfig& c) {
        if (body["mqtt_host"].is<const char*>())
            strlcpy(c.mqtt_host, body["mqtt_host"], sizeof(c.mqtt_host));
        if (body["mqtt_port"].is<int>())
            c.mqtt_port = static_cast<uint16_t>(std::clamp<int>(body["mqtt_port"], 1, 65535));
        if (body["mqtt_username"].is<const char*>())
            strlcpy(c.mqtt_username, body["mqtt_username"], sizeof(c.mqtt_username));
        // Empty/absent = leave unchanged; the GET side never echoes the real secret.
        if (body["mqtt_password"].is<const char*>() && strlen(body["mqtt_password"]) > 0)
            strlcpy(c.mqtt_password, body["mqtt_password"], sizeof(c.mqtt_password));
        if (body["mqtt_use_tls"].is<bool>()) c.mqtt_use_tls = body["mqtt_use_tls"];
        if (body["mqtt_validate_cert"].is<bool>()) c.mqtt_validate_cert = body["mqtt_validate_cert"];
        if (body["brightness_pct"].is<int>())
            c.brightness_pct = static_cast<uint8_t>(std::clamp<int>(body["brightness_pct"], 0, 100));
        if (body["screen_timeout_s"].is<int>()) c.screen_timeout_s = body["screen_timeout_s"];
        if (body["rotation"].is<int>()) c.rotation = body["rotation"];
        if (body["dark_mode"].is<bool>()) c.dark_mode = body["dark_mode"];
        if (body["dark_mode_schedule"].is<int>()) c.dark_mode_schedule = body["dark_mode_schedule"];
        if (body["schedule_dark_start_hour"].is<int>())
            c.schedule_dark_start_hour = body["schedule_dark_start_hour"];
        if (body["schedule_dark_start_min"].is<int>())
            c.schedule_dark_start_min = body["schedule_dark_start_min"];
        if (body["schedule_light_start_hour"].is<int>())
            c.schedule_light_start_hour = body["schedule_light_start_hour"];
        if (body["schedule_light_start_min"].is<int>())
            c.schedule_light_start_min = body["schedule_light_start_min"];
        if (body["show_flow_chart"].is<bool>()) c.show_flow_chart = body["show_flow_chart"];
        if (body["ota_auto_check"].is<bool>()) c.ota_auto_check = body["ota_auto_check"];
        if (body["ota_auto_install"].is<bool>()) c.ota_auto_install = body["ota_auto_install"];
        if (body["ota_manifest_url"].is<const char*>())
            strlcpy(c.ota_manifest_url, body["ota_manifest_url"], sizeof(c.ota_manifest_url));
        if (body["chargepoint_ids"].is<JsonArray>()) {
            JsonArray ids = body["chargepoint_ids"];
            uint8_t count = std::min<size_t>(ids.size(), config::AppConfig::kMaxChargepoints);
            for (uint8_t i = 0; i < count; ++i) c.chargepoint_ids[i] = ids[i];
            c.chargepoint_count = count;
        }
    });

    if (body["brightness_pct"].is<int>()) {
        board::SetBacklight(static_cast<uint8_t>(std::clamp<int>(body["brightness_pct"], 0, 100)));
    }
    // Caller sends the response; CheckAuth() already did on failure.
    return true;
}

// Separate from /api/config since an empty value here means "clear the PIN", unlike
// /api/config's "empty means leave unchanged" for mqtt_password.
bool HandleSetWebUiPin(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!CheckAuth(request)) return false;
    JsonObject body = json.as<JsonObject>();
    const char* new_pin = body["pin"] | "";
    // The on-device keypad (ui::RequirePin) can only ever enter digits — a PIN
    // containing anything else, set here from a full keyboard, would lock the user out
    // of the on-device settings screen/WiFi-reset with no way back short of reflashing.
    for (const char* p = new_pin; *p != '\0'; ++p) {
        if (!std::isdigit(static_cast<unsigned char>(*p))) {
            request->send(400, "application/json", "{\"error\":\"PIN darf nur aus Ziffern bestehen\"}");
            return false;
        }
    }
    config::Store().Update(
        [&](config::AppConfig& c) { strlcpy(c.web_ui_pin, new_pin, sizeof(c.web_ui_pin)); });
    return true;
}

void HandleMqttDiscoverStart(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    net::StartBrokerDiscovery();
    request->send(200, "application/json", "{\"started\":true}");
}

void HandleMqttDiscoverProgress(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    JsonDocument doc;
    doc["running"] = net::IsBrokerDiscoveryRunning();
    doc["progress_percent"] = net::BrokerDiscoveryProgressPercent();
    JsonArray found = doc["brokers"].to<JsonArray>();
    uint8_t count = net::DiscoveredBrokerCount();
    for (uint8_t i = 0; i < count; ++i) {
        net::DiscoveredBroker broker = net::GetDiscoveredBroker(i);
        JsonObject obj = found.add<JsonObject>();
        obj["host"] = broker.host;
        obj["port"] = broker.port;
    }
    SendJson(request, doc);
}

void HandleWifiReset(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    // Must respond before ResetAndReprovision(), which reboots the device.
    request->send(200, "application/json", "{\"success\":true}");
    net::ResetAndReprovision();
}

void HandleResetSettings(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    config::Store().ResetSettingsPreservingConnection();
    // Doesn't restart itself — the frontend follows up with /api/restart, same as a
    // normal save, so this behaves identically to "reset the form, then save & restart".
    request->send(200, "application/json", "{\"success\":true}");
}

void HandleRestart(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    // Must respond before restarting — unlike /api/wifi/reset, this keeps WiFi/MQTT
    // config as-is; it's just how config changes (e.g. chargepoint_ids) take effect
    // without a physical power-cycle.
    request->send(200, "application/json", "{\"success\":true}");
    delay(300);
    ESP.restart();
}

void HandleGetInfo(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    JsonDocument doc;
    doc["project_name"] = PROJECT_NAME;
    doc["firmware_version"] = FW_VERSION;
    doc["uptime_s"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["heap_size"] = ESP.getHeapSize();
    doc["free_psram"] = ESP.getPsramSize() > 0 ? ESP.getFreePsram() : 0;
    doc["psram_size"] = ESP.getPsramSize();
    doc["chip_model"] = ESP.getChipModel();
    doc["chip_temp_c"] = temperatureRead(); // the chip's own die temperature, not ambient
    doc["flash_size"] = ESP.getFlashChipSize();
    doc["fs_used_bytes"] = LittleFS.usedBytes();
    doc["fs_total_bytes"] = LittleFS.totalBytes();
    doc["wifi_rssi_dbm"] = WiFi.RSSI();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mqtt_connected"] = state::State().IsMqttConnected();

    SendJson(request, doc);
}

void HandleOtaCheck(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    ota::UpdateCheckResult result = ota::CheckForUpdate(/*auto_triggered=*/false);

    JsonDocument doc;
    doc["success"] = result.success;
    doc["update_available"] = result.update_available;
    doc["current_version"] = result.current_version;
    doc["latest_version"] = result.latest_version;
    doc["message"] = result.message;

    SendJson(request, doc);
}

void HandleOtaStart(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    bool force = request->hasParam("force", true) && request->getParam("force", true)->value() == "1";
    ota::InstallResult result = ota::StartInstall(force);

    JsonDocument doc;
    doc["started"] = result.started;
    doc["reason"] = result.reason;

    SendJson(request, doc);
}

void HandleOtaProgress(AsyncWebServerRequest* request) {
    if (!CheckAuth(request)) return;
    JsonDocument doc;
    doc["progress_percent"] = ota::InstallProgressPercent();
    doc["failure_reason"] = ota::InstallFailureReason();

    SendJson(request, doc);
}

} // namespace

void RegisterApiRoutes(AsyncWebServer& server) {
    server.on("/api/status", HTTP_GET, HandleGetStatus);
    server.on("/api/chargepoints/discovered", HTTP_GET, HandleGetDiscoveredChargepoints);
    server.on("/api/config", HTTP_GET, HandleGetConfig);
    server.on("/api/info", HTTP_GET, HandleGetInfo);
    server.on("/api/ota/check", HTTP_POST, HandleOtaCheck);
    server.on("/api/ota/start", HTTP_POST, HandleOtaStart);
    server.on("/api/ota/progress", HTTP_GET, HandleOtaProgress);
    server.on("/api/mqtt/discover", HTTP_POST, HandleMqttDiscoverStart);
    server.on("/api/mqtt/discover/progress", HTTP_GET, HandleMqttDiscoverProgress);
    server.on("/api/wifi/reset", HTTP_POST, HandleWifiReset);
    server.on("/api/config/reset", HTTP_POST, HandleResetSettings);
    server.on("/api/restart", HTTP_POST, HandleRestart);

    auto* config_post_handler = new AsyncCallbackJsonWebHandler(
        "/api/config",
        [](AsyncWebServerRequest* request, JsonVariant json) {
            if (HandlePostConfig(request, json)) request->send(200, "application/json", "{\"success\":true}");
        });
    server.addHandler(config_post_handler);

    auto* pin_post_handler = new AsyncCallbackJsonWebHandler(
        "/api/webui/pin",
        [](AsyncWebServerRequest* request, JsonVariant json) {
            if (HandleSetWebUiPin(request, json)) request->send(200, "application/json", "{\"success\":true}");
        });
    server.addHandler(pin_post_handler);
}

} // namespace web
