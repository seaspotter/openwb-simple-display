// WiFi captive-portal provisioning (tzapu/WiFiManager), also collecting the MQTT broker
// host/port/username/password in the same first-boot flow.
#pragma once

#include <cstddef>

namespace net {

// Deterministic, not random: derived from the chip's own efuse MAC, so it's the same
// every time (across reboots of the same unit, and independently computed by the UI
// task to show on screen_provisioning — see lvgl_port.cpp) without needing any shared
// state between the two. 8 digits — the minimum WPA2 passphrase length — written into
// `out` (must be at least 9 bytes).
void GetApPassword(char* out, size_t out_size);

// Connects using saved WiFi credentials, or starts the captive portal and blocks until
// provisioned. Call once from net_task's startup.
void Provision();

// Clears saved WiFi credentials and reboots into the captive portal.
void ResetAndReprovision();

} // namespace net
