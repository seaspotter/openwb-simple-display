#include "lvgl_port.h"

#include <Arduino.h>
#include <lvgl.h>

#include "board/display_driver.h"
#include "board/touch_driver.h"
#include "config/config_store.h"
#include "state/app_state.h"
#include "theme/theme_quartz.h"
#include "screens/screen_boot.h"
#include "util/logging.h"

namespace ui {
namespace {

constexpr char kTag[] = "lvgl_port";
constexpr uint32_t kTickIntervalMs = 5;
constexpr uint32_t kTimeoutCheckIntervalMs = 200;
TaskHandle_t g_task_handle = nullptr;

void ApplyScreenTimeout(bool* dimmed) {
    const config::AppConfig& cfg = config::Store().Get();
    if (cfg.screen_timeout_s == 0) {
        if (*dimmed) {
            *dimmed = false;
            board::SetBacklight(cfg.brightness_pct);
        }
        return;
    }

    bool idle = millis() - state::State().LastTouchMillis() >= cfg.screen_timeout_s * 1000UL;
    if (idle != *dimmed) {
        *dimmed = idle;
        board::SetBacklight(idle ? 0 : cfg.brightness_pct);
    }
}

void LvglTask(void* /*param*/) {
    uint32_t last_tick = millis();
    uint32_t last_timeout_check = last_tick;
    bool dimmed = false;
    for (;;) {
        uint32_t now = millis();
        lv_tick_inc(now - last_tick);
        last_tick = now;
        lv_timer_handler();

        if (now - last_timeout_check >= kTimeoutCheckIntervalMs) {
            last_timeout_check = now;
            ApplyScreenTimeout(&dimmed);
        }

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
    state::State().NoteTouch(millis());

    theme::ApplyQuartzTheme(display, cfg.dark_mode);
    screens::ShowBootScreen();

    // Core 1 for UI; net_task runs on core 0 so network work never stalls rendering.
    xTaskCreatePinnedToCore(LvglTask, "lvgl", 8192, nullptr, 2, &g_task_handle, 1);
}

} // namespace ui
