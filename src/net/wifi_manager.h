// WiFi captive-portal provisioning via tzapu/WiFiManager (plan §4) — not a hand-rolled
// DNS-redirect implementation. Collects the MQTT broker host/port/username/password in
// the same first-boot flow as WiFi credentials via WiFiManager's custom-parameter
// support.
#pragma once

namespace net {

// Connects using AppConfig's saved WiFi credentials; if none are saved or connecting
// fails, starts the captive-portal AP (SSID: "<device_name><AP_SSID_SUFFIX>") and blocks
// until the user submits the form, writing the result into config::Store() and
// rebooting. Call once from net_task's startup, before openwb::mqtt::Init()/web_server::Init().
void Provision();

// Clears saved WiFi credentials and reboots into the captive portal. Wired to
// ui::ShowWifiResetPrompt() (a long-press on the status screen, since WiFi reset is the
// one setting that can't be web-UI-only) and to a button in the web UI.
void ResetAndReprovision();

} // namespace net
