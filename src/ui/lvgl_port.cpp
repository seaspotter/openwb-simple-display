#include "lvgl_port.h"

#include <Arduino.h>
#include <lvgl.h>

#include "board/display_driver.h"
#include "board/touch_driver.h"
#include "config/config_store.h"
#include "theme/theme_quartz.h"
#include "screens/screen_boot.h"
#include "util/logging.h"

namespace ui {
namespace {

constexpr char kTag[] = "lvgl_port";
constexpr uint32_t kTickIntervalMs = 5;
TaskHandle_t g_task_handle = nullptr;

void LvglTask(void* /*param*/) {
    uint32_t last_tick = millis();
    for (;;) {
        uint32_t now = millis();
        lv_tick_inc(now - last_tick);
        last_tick = now;
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(kTickIntervalMs));
    }
}

} // namespace

void Init() {
    lv_display_t* display = board::InitDisplay();
    if (display == nullptr) {
        util::LogError(kTag, "display init failed");
        return;
    }

    const config::AppConfig& cfg = config::Store().Get();
    board::SetTouchRotation(cfg.rotation);
    board::SetBacklight(cfg.brightness_pct);

    theme::ApplyQuartzTheme(display, cfg.dark_mode);
    screens::ShowBootScreen();

    // Core 1 is dedicated to UI; the network task (src/net/net_task.*) runs on core 0 so
    // HTTP/WiFi work never stalls rendering or touch handling (plan §5).
    xTaskCreatePinnedToCore(LvglTask, "lvgl", 8192, nullptr, 2, &g_task_handle, 1);
}

} // namespace ui
