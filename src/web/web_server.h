// ESPAsyncWebServer lifecycle. Only active in station mode; WiFiManager owns port 80
// during provisioning.
#pragma once

namespace web {

// Starts the server, static asset serving, API routes, and mDNS. Call once WiFi is up.
void InitServer();

} // namespace web
