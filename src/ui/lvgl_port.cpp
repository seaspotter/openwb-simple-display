#include "lvgl_port.h"

#include <ctime>

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
#include "util/sun_times.h"

namespace ui {
namespace {

constexpr char kTag[] = "lvgl_port";
constexpr uint32_t kTickIntervalMs = 5;
constexpr uint32_t kTimeoutCheckIntervalMs = 200;
constexpr uint32_t kScheduleCheckIntervalMs = 30000;
// Hardcoded on purpose, per the project's own choice — accurate enough for a day/light
// decision anywhere in Germany, without asking the person for their exact location.
constexpr double kMiddleGermanyLat = 51.1657;
constexpr double kMiddleGermanyLon = 10.4515;
TaskHandle_t g_task_handle = nullptr;
lv_display_t* g_display = nullptr;

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

// Flips dark_mode live, no reboot, on whatever cadence the "Automatischer Wechsel"
// setting asks for — manual leaves dark_mode solely under the "Darstellung" dropdown.
// Only re-renders the active screen when it's the status screen; if the person happens
// to be in settings at the exact moment a transition fires, the new theme applies the
// next time the status screen is shown instead of interrupting them.
void ApplyDarkModeSchedule(ScreenMode screen_mode) {
    const config::AppConfig& cfg = config::Store().Get();
    auto schedule = static_cast<config::DarkModeSchedule>(cfg.dark_mode_schedule);
    if (schedule == config::DarkModeSchedule::kManual) return;

    time_t now_time = time(nullptr);
    struct tm local_tm;
    localtime_r(&now_time, &local_tm);
    // SNTP hasn't synced yet (default epoch) — don't act on a bogus "1970" clock.
    if (local_tm.tm_year < 120) return;

    bool should_be_dark;
    if (schedule == config::DarkModeSchedule::kTimeRange) {
        should_be_dark =
            util::IsDarkByTimeRange(local_tm.tm_hour, local_tm.tm_min, cfg.schedule_dark_start_hour,
                                     cfg.schedule_dark_start_min, cfg.schedule_light_start_hour,
                                     cfg.schedule_light_start_min);
    } else {
        // tm_gmtoff isn't available in this toolchain's libc by default, and this
        // project's timezone is hardcoded to Europe/Berlin anyway (see time_sync.cpp),
        // so tm_isdst alone unambiguously picks CET (+1) vs CEST (+2).
        double utc_offset_hours = local_tm.tm_isdst > 0 ? 2.0 : 1.0;
        should_be_dark = util::IsDarkBySunriseSunset(local_tm.tm_hour, local_tm.tm_min, local_tm.tm_yday + 1,
                                                      kMiddleGermanyLat, kMiddleGermanyLon, utc_offset_hours);
    }
    if (should_be_dark == cfg.dark_mode) return;

    config::Store().Update([should_be_dark](config::AppConfig& c) { c.dark_mode = should_be_dark; });
    theme::ApplyQuartzTheme(g_display, should_be_dark);
    if (screen_mode == ScreenMode::kStatus) {
        screens::ShowStatusScreen(config::Store().Get().chargepoint_count);
    }
}

void LvglTask(void* /*param*/) {
    uint32_t last_tick = millis();
    uint32_t last_timeout_check = last_tick;
    uint32_t last_schedule_check = last_tick;
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

        if (now - last_schedule_check >= kScheduleCheckIntervalMs) {
            last_schedule_check = now;
            ApplyDarkModeSchedule(screen_mode);
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
    g_display = display;

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
