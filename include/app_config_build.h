// Compile-time constants. Runtime-editable settings live in src/config/app_config.h instead.
#pragma once

#ifndef PROJECT_NAME
#define PROJECT_NAME "openwb-simple-display"
#endif

#define FW_VERSION "0.1.4"

// Captive-portal AP SSID shown on first boot / after a WiFi reset.
#define AP_SSID_SUFFIX "-setup" // -> "openwb-simple-display-setup"

// Default device-facing OTA manifest, hosted on the project's GitHub Pages site.
// Overridable at runtime via AppConfig::ota_manifest_url.
#define DEFAULT_OTA_MANIFEST_URL \
    "https://seaspotter.github.io/openwb-simple-display/firmware/ota/manifest.json"
