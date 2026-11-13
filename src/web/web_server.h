// ESPAsyncWebServer lifecycle (plan §7). Only ever active in normal (station) mode —
// WiFiManager owns port 80 during the provisioning window, and the two never run
// concurrently within one boot cycle, so there's no port conflict to guard against here.
#pragma once

namespace web {

// Starts the server, registers static asset serving from LittleFS and the /api/* routes
// (api_routes.cpp), and begins mDNS advertisement at "<device_name>.local". Call once
// from net_task, after WiFi is confirmed connected.
void InitServer();

} // namespace web
