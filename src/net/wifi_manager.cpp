#include "wifi_manager.h"

#include <WiFiManager.h>

#include "app_config_build.h"
#include "config/config_store.h"
#include "util/logging.h"

namespace net {
namespace {
constexpr char kTag[] = "wifi_manager";
bool g_params_saved = false;

void OnSaveParams() { g_params_saved = true; }

} // namespace

void Provision() {
    const config::AppConfig& cfg = config::Store().Get();

    WiFiManager wm;
    wm.setSaveParamsCallback(OnSaveParams);

    // Custom fields collected in the same first-boot flow as WiFi credentials (plan
    // §4), pre-filled with whatever's already configured so re-entering setup doesn't
    // lose existing values. TLS on/off is deliberately left out of this minimal portal —
    // it defaults to the broker's plaintext listener (confirmed to work against the
    // live instance) and can be switched on later via the full web settings UI, which
    // fits this device's "intentionally thin on-device provisioning" scope better than
    // a checkbox typed on a phone keyboard.
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

    String ap_ssid = String(cfg.device_name) + AP_SSID_SUFFIX;
    bool connected = wm.autoConnect(ap_ssid.c_str());

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
            // WiFi password itself is not duplicated into AppConfig: WiFiManager
            // persists it via the ESP32 WiFi stack's own NVS storage, and re-reading it
            // back out isn't needed for anything this app does.
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
