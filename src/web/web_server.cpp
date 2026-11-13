#include "web_server.h"

#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#include "api_routes.h"
#include "config/config_store.h"
#include "util/logging.h"

namespace web {
namespace {
constexpr char kTag[] = "web_server";
AsyncWebServer g_server(80);
} // namespace

void InitServer() {
    if (!LittleFS.begin()) {
        util::LogError(kTag, "LittleFS mount failed — static web assets unavailable");
    }

    const config::AppConfig& cfg = config::Store().Get();
    if (MDNS.begin(cfg.device_name)) {
        MDNS.addService("http", "tcp", 80);
    } else {
        util::LogWarn(kTag, "mDNS init failed");
    }

    RegisterApiRoutes(g_server);

    // src/web/www/ is mirrored into data/ at build time.
    g_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    g_server.begin();
    util::LogInfo(kTag, "web server started");
}

} // namespace web
