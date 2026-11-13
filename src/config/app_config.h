// Persistent device settings. Single schema shared by the captive-portal provisioning
// flow, the on-device settings screen, and the web settings UI (plan §3) — all three read
// and write through config::Store(), never parse/validate this shape independently.
#pragma once

#include <cstdint>

#include "app_config_build.h"

namespace config {

// Bump when adding/removing/retyping a field; config_store.cpp::Migrate() handles
// forward migration from any older schema_version found in NVS.
inline constexpr uint8_t kCurrentSchemaVersion = 1;

struct AppConfig {
    uint8_t schema_version = kCurrentSchemaVersion;

    // WiFi. (WiFiManager may end up owning its own NVS namespace for these instead of
    // this struct — decide during M2; fields kept here in the meantime so the rest of
    // the app has a single place to read "are we provisioned".)
    char wifi_ssid[33] = "";
    char wifi_password[65] = "";

    // openWB MQTT broker connection (plan §5) — the sole backend transport.
    char mqtt_host[64] = "";
    uint16_t mqtt_port = 1883;       // 1883 plaintext (default); 8883 when mqtt_use_tls
    char mqtt_username[33] = "";     // empty = anonymous connect (confirmed to work
                                      // against openWB's default "unsecure" broker config)
    char mqtt_password[65] = "";
    bool mqtt_use_tls = false;
    bool mqtt_validate_cert = false; // openWB's broker TLS cert is self-signed

    // Chargepoints.
    static constexpr uint8_t kMaxChargepoints = 4; // open item, plan §11
    uint8_t chargepoint_ids[kMaxChargepoints] = {1, 0, 0, 0};
    uint8_t chargepoint_count = 1;

    // Display.
    uint8_t brightness_pct = 80;
    uint16_t screen_timeout_s = 120; // 0 = never
    uint16_t rotation = 0;           // degrees: 0/90/180/270 — doesn't fit in uint8_t
    bool dark_mode = false;

    // OTA (plan §9) — background checks only ever notify; install is always an explicit
    // user action from the web UI or the on-device settings screen.
    bool ota_auto_check = true;
    char ota_manifest_url[128] = DEFAULT_OTA_MANIFEST_URL;
    uint32_t ota_check_interval_s = 21600; // 6h

    char device_name[32] = PROJECT_NAME;
};

} // namespace config
