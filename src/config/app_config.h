// Persistent device settings, shared by the captive portal and the web UI via
// config::Store().
#pragma once

#include <cstdint>

#include "app_config_build.h"

namespace config {

// Bump on any field add/remove/retype; config_store.cpp::Migrate() handles the rest.
inline constexpr uint8_t kCurrentSchemaVersion = 3; // v3: dropped extra_meter_id, added show_flow_chart

struct AppConfig {
    uint8_t schema_version = kCurrentSchemaVersion;

    char wifi_ssid[33] = "";
    char wifi_password[65] = "";

    char mqtt_host[64] = "";
    uint16_t mqtt_port = 1883;
    char mqtt_username[33] = ""; // empty = anonymous connect
    char mqtt_password[65] = "";
    bool mqtt_use_tls = false;
    bool mqtt_validate_cert = false; // broker's TLS cert is typically self-signed

    // Local web UI HTTP Basic Auth; empty = disabled. Set only via POST /api/webui/pin.
    // No recovery if forgotten short of reflashing with "Erase device".
    char web_ui_pin[17] = "";

    static constexpr uint8_t kMaxChargepoints = 4;
    uint8_t chargepoint_ids[kMaxChargepoints] = {1, 0, 0, 0};
    uint8_t chargepoint_count = 1;

    uint8_t brightness_pct = 80;
    uint16_t screen_timeout_s = 0; // seconds of inactivity before dimming off; 0 = never
    uint16_t rotation = 0;           // degrees: 0/90/180/270
    bool dark_mode = false;
    bool show_flow_chart = true; // the PV/grid/battery/... flow bars above the chargepoint pager

    bool ota_auto_check = true; // background checks only notify, never auto-install
    char ota_manifest_url[128] = DEFAULT_OTA_MANIFEST_URL;
    uint32_t ota_check_interval_s = 21600;

    char device_name[32] = PROJECT_NAME;
};

} // namespace config
