// Boot sequence: bring up the display/touch/LVGL stack, then hand off to the net task
// for WiFi provisioning, the MQTT client, the web settings server, and OTA checks — see
// /home/seaspotter/.claude/plans/clever-sprouting-cray.md for the full architecture.
#include <Arduino.h>

#include "net/net_task.h"
#include "ui/lvgl_port.h"
#include "ui/ui_events.h"
#include "util/logging.h"

namespace {
constexpr char kTag[] = "main";
}

void setup() {
    Serial.begin(115200);
    util::LogInfo(kTag, "booting");

    ui::Init();
    ui::InitEvents();
    net::Init();
}

void loop() {
    // Intentionally empty: the LVGL task (core 1) and, from M2 on, the net task (core 0)
    // do all real work. Nothing belongs in Arduino's default loop task for this project.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
