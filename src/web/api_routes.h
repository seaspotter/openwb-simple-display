// REST-ish JSON endpoints backing the web settings UI (plan §7). Registered onto the
// shared AsyncWebServer instance by web_server.cpp.
#pragma once

#include <ESPAsyncWebServer.h>

namespace web {

void RegisterApiRoutes(AsyncWebServer& server);

} // namespace web
