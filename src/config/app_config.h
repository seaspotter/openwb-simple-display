// Persistent device settings, shared by the captive portal and the web UI via
// config::Store().
#pragma once

#include <cstdint>

#include "app_config_build.h"

namespace config {

// Bump on any field add/remove/retype — purely informational now: config_store.cpp's
// Load() reads the overlapping byte prefix regardless, so a field add/remove still
// requires fields to only ever be appended at the end of this struct, never reordered
// or resized, or older devices' saved bytes would land in the wrong field on upgrade.
inline constexpr uint8_t kCurrentSchemaVersion = 5; // v5: added ota_auto_install

enum class DarkModeSchedule : uint8_t {
    kManual = 0,       // "Darstellung" dropdown is the only thing that sets dark_mode
    kTimeRange = 1,    // schedule_dark_start_* / schedule_light_start_*
    kSunriseSunset = 2, // calculated daily (see util/sun_times.h); fixed middle-of-Germany location
};

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

    // Automatically flips dark_mode on a schedule, live, no reboot needed — see
    // util/sun_times.h and ui/lvgl_port.cpp's periodic check. Manual (0) leaves dark_mode
    // solely under the "Darstellung" dropdown above.
    uint8_t dark_mode_schedule = static_cast<uint8_t>(DarkModeSchedule::kManual);
    uint8_t schedule_dark_start_hour = 22;
    uint8_t schedule_dark_start_min = 0;
    uint8_t schedule_light_start_hour = 7;
    uint8_t schedule_light_start_min = 0;

    bool ota_auto_check = true; // background checks only notify, unless ota_auto_install too
    char ota_manifest_url[128] = DEFAULT_OTA_MANIFEST_URL;
    uint32_t ota_check_interval_s = 21600;

    char device_name[32] = PROJECT_NAME;

    // Opt-in: if an ota_auto_check finds an update, install it right away instead of
    // only showing the status-bar icon. Still subject to the same idle/not-charging
    // gate as a manual install (ota_gate.cpp) — never installs while a chargepoint is
    // charging or the display was touched recently.
    bool ota_auto_install = false;
};

} // namespace config
