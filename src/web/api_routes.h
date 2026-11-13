// JSON API endpoints backing the web settings UI.
#pragma once

#include <ESPAsyncWebServer.h>

namespace web {

void RegisterApiRoutes(AsyncWebServer& server);

} // namespace web
