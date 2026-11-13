// WiFi captive-portal provisioning (tzapu/WiFiManager), also collecting the MQTT broker
// host/port/username/password in the same first-boot flow.
#pragma once

namespace net {

// Connects using saved WiFi credentials, or starts the captive portal and blocks until
// provisioned. Call once from net_task's startup.
void Provision();

// Clears saved WiFi credentials and reboots into the captive portal.
void ResetAndReprovision();

} // namespace net
