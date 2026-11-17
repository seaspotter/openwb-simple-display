#include "lvgl_port.h"

#include <Arduino.h>
#include <lvgl.h>

#include "app_config_build.h"
#include "board/display_driver.h"
#include "board/touch_driver.h"
#include "config/config_store.h"
#include "state/app_state.h"
#include "theme/theme_quartz.h"
#include "screens/screen_boot.h"
#include "screens/screen_provisioning.h"
#include "screens/screen_status.h"
#include "util/logging.h"

namespace ui {
namespace {

constexpr char kTag[] = "lvgl_port";
constexpr uint32_t kTickIntervalMs = 5;
constexpr uint32_t kTimeoutCheckIntervalMs = 200;
TaskHandle_t g_task_handle = nullptr;

enum class ScreenMode { kBoot, kProvisioning, kStatus };

// Moves off the initial boot screen once the net task has news: either the captive
// portal is up (no saved WiFi) or normal operation has started (network_ready). Network
// readiness always wins — it means WiFi is up and the portal, if it ever showed, is done.
void ApplyScreenTransition(ScreenMode* mode) {
    if (*mode == ScreenMode::kStatus) return;

    if (state::State().IsNetworkReady()) {
        *mode = ScreenMode::kStatus;
        screens::ShowStatusScreen(config::Store().Get().chargepoint_count);
        return;
    }

    if (*mode == ScreenMode::kBoot && state::State().IsProvisioningActive()) {
        *mode = ScreenMode::kProvisioning;
        String ap_ssid = String(config::Store().Get().device_name) + AP_SSID_SUFFIX;
        screens::ShowProvisioningScreen(ap_ssid.c_str());
    }
}

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
    ScreenMode screen_mode = ScreenMode::kBoot;
    for (;;) {
        uint32_t now = millis();
        lv_tick_inc(now - last_tick);
        last_tick = now;
        lv_timer_handler();

        if (now - last_timeout_check >= kTimeoutCheckIntervalMs) {
            last_timeout_check = now;
            ApplyScreenTimeout(&dimmed);
            ApplyScreenTransition(&screen_mode);
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
