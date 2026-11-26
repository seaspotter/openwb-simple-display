#include "lvgl_port.h"

#include <ctime>

#include <Arduino.h>
#include <lvgl.h>

#include "app_config_build.h"
#include "board/display_driver.h"
#include "board/touch_driver.h"
#include "config/config_store.h"
#include "net/wifi_manager.h"
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
// How long the backlight stays on after a touch while inside a sleep-schedule window,
// independent of screen_timeout_s (which may be 0, "never", for daytime use — using it
// here too would mean a sleep schedule configured alongside an inactivity timeout of
// "never" could never actually re-darken after a touch, defeating the schedule).
constexpr uint32_t kScheduledSleepWakeGraceMs = 15000;
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
        char ap_password[9];
        net::GetApPassword(ap_password, sizeof(ap_password));
        screens::ShowProvisioningScreen(ap_ssid.c_str(), ap_password);
    }
}

// True if `sleep_schedule_enabled` and the current local time falls inside
// [sleep_start, wake_start) — reuses the dark-mode schedule's own time-range test
// (util::IsDarkByTimeRange), since "is it within a wrapping evening-to-morning window" is
// exactly the same calculation either way, just against different config fields.
bool IsWithinSleepSchedule(const config::AppConfig& cfg) {
    if (!cfg.sleep_schedule_enabled) return false;

    time_t now_time = time(nullptr);
    struct tm local_tm;
    localtime_r(&now_time, &local_tm);
    if (local_tm.tm_year < 120) return false; // SNTP hasn't synced yet (default epoch)

    return util::IsDarkByTimeRange(local_tm.tm_hour, local_tm.tm_min, cfg.sleep_start_hour, cfg.sleep_start_min,
                                    cfg.wake_start_hour, cfg.wake_start_min);
}

// Two independent reasons the backlight can be off, combined with OR so neither setting
// disables the other: screen_timeout_s's plain inactivity timeout (unchanged from
// before), and a configured sleep schedule's own, shorter wake-grace timeout while inside
// its window. A touch always wins immediately either way — idle_ms resets to ~0 on every
// touch (see state::NoteTouch, called from the touch driver), which is below both
// thresholds the instant it happens.
void ApplyScreenTimeout(bool* dimmed) {
    const config::AppConfig& cfg = config::Store().Get();
    uint32_t idle_ms = millis() - state::State().LastTouchMillis();

    bool idle_timeout = cfg.screen_timeout_s != 0 && idle_ms >= cfg.screen_timeout_s * 1000UL;
    bool scheduled_sleep = IsWithinSleepSchedule(cfg) && idle_ms >= kScheduledSleepWakeGraceMs;
    bool should_be_dimmed = idle_timeout || scheduled_sleep;

    if (should_be_dimmed != *dimmed) {
        *dimmed = should_be_dimmed;
        board::SetBacklight(should_be_dimmed ? 0 : cfg.brightness_pct);
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
