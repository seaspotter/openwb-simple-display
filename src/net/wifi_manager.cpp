#include "wifi_manager.h"

#include <cstdio>

#include <WiFiManager.h>

#include "app_config_build.h"
#include "config/config_store.h"
#include "state/app_state.h"
#include "util/logging.h"

namespace net {
namespace {
constexpr char kTag[] = "wifi_manager";
bool g_params_saved = false;

void OnSaveParams() { g_params_saved = true; }

} // namespace

void GetApPassword(char* out, size_t out_size) {
    snprintf(out, out_size, "%08u", static_cast<unsigned>(ESP.getEfuseMac() % 100000000ULL));
}

void Provision() {
    const config::AppConfig& cfg = config::Store().Get();

    WiFiManager wm;
    wm.setSaveParamsCallback(OnSaveParams);

    // TLS is intentionally not offered here; it defaults off and is set via the web UI.
    char port_buf[6];
    snprintf(port_buf, sizeof(port_buf), "%u", cfg.mqtt_port);

    WiFiManagerParameter host_param("mqtt_host", "MQTT-Broker Host oder IP", cfg.mqtt_host,
                                     sizeof(cfg.mqtt_host) - 1);
    WiFiManagerParameter port_param("mqtt_port", "MQTT-Port", port_buf, sizeof(port_buf) - 1);
    WiFiManagerParameter username_param("mqtt_user", "MQTT-Benutzername (optional)", cfg.mqtt_username,
                                         sizeof(cfg.mqtt_username) - 1);
    WiFiManagerParameter password_param("mqtt_pass", "MQTT-Passwort (optional)", cfg.mqtt_password,
                                         sizeof(cfg.mqtt_password) - 1);

    wm.addParameter(&host_param);
    wm.addParameter(&port_param);
    wm.addParameter(&username_param);
    wm.addParameter(&password_param);

    // Fires only if there's no saved WiFi to connect to directly — tells the UI task
    // (lvgl_port.cpp) to show the provisioning screen instead of staying on "Verbinde...".
    wm.setAPCallback([](WiFiManager*) { state::State().SetProvisioningActive(true); });

    String ap_ssid = String(cfg.device_name) + AP_SSID_SUFFIX;
    char ap_password[9];
    GetApPassword(ap_password, sizeof(ap_password));
    bool connected = wm.autoConnect(ap_ssid.c_str(), ap_password);

    if (!connected) {
        util::LogError(kTag, "WiFi provisioning timed out without a connection, restarting");
        delay(1000);
        ESP.restart();
        return;
    }

    if (g_params_saved) {
        config::Store().Update([&](config::AppConfig& c) {
            strlcpy(c.mqtt_host, host_param.getValue(), sizeof(c.mqtt_host));
            c.mqtt_port = static_cast<uint16_t>(atoi(port_param.getValue()));
            strlcpy(c.mqtt_username, username_param.getValue(), sizeof(c.mqtt_username));
            strlcpy(c.mqtt_password, password_param.getValue(), sizeof(c.mqtt_password));
            strlcpy(c.wifi_ssid, WiFi.SSID().c_str(), sizeof(c.wifi_ssid));
            // WiFi password stays in the ESP32 WiFi stack's own NVS, not duplicated here.
        });
        util::LogInfo(kTag, "provisioning saved, rebooting into normal operation");
        delay(500);
        ESP.restart();
    }
}

void ResetAndReprovision() {
    WiFiManager wm;
    wm.resetSettings();
    delay(500);
    ESP.restart();
}

} // namespace net
