#include "status_bar.h"

#include <cstdio>
#include <ctime>

#include "config/config_store.h"
#include "ota/ota_manager.h"
#include "state/app_state.h"
#include "ui/screens/screen_settings.h"
#include "ui/theme/fonts/fonts.h"
#include "ui/theme/theme_colors.h"
#include "ui/widgets/layout_helpers.h"

namespace widgets {
namespace {

constexpr uint32_t kRefreshIntervalMs = 1000;

struct StatusBarWidgets {
    lv_obj_t* time_label;
    lv_obj_t* issue_icon;
    lv_obj_t* update_icon;
    lv_obj_t* connection_icon;
    lv_timer_t* refresh_timer;
};

void Refresh(StatusBarWidgets* widgets) {
    const config::AppConfig& cfg = config::Store().Get();
    const theme::Palette& colors = theme::Colors(cfg.dark_mode);

    time_t now = time(nullptr);
    struct tm local_tm;
    localtime_r(&now, &local_tm);
    if (local_tm.tm_year > 100) { // before NTP sync, time(nullptr) reports ~1970
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", local_tm.tm_hour, local_tm.tm_min);
        lv_label_set_text(widgets->time_label, buf);
    } else {
        lv_label_set_text(widgets->time_label, "--:--");
    }

    bool connected = state::State().IsMqttConnected();
    char conn_utf8[4];
    icon::Utf8Encode(connected ? icon::Glyph::kWifi : icon::Glyph::kWifiOff, conn_utf8);
    lv_label_set_text(widgets->connection_icon, conn_utf8);
    lv_obj_set_style_text_color(widgets->connection_icon, connected ? colors.muted : colors.error,
                                 LV_PART_MAIN);

    if (ota::IsUpdateAvailable()) {
        lv_obj_remove_flag(widgets->update_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(widgets->update_icon, LV_OBJ_FLAG_HIDDEN);
    }

    bool any_fault = false;
    for (uint8_t i = 0; i < cfg.chargepoint_count && i < config::AppConfig::kMaxChargepoints; ++i) {
        if (state::State().GetChargepointStatus(i).fault_state != 0) {
            any_fault = true;
            break;
        }
    }
    if (any_fault) {
        lv_obj_remove_flag(widgets->issue_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(widgets->issue_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void RefreshTick(lv_timer_t* timer) {
    Refresh(static_cast<StatusBarWidgets*>(lv_timer_get_user_data(timer)));
}

void OnSettingsTapped(lv_event_t* /*e*/) { screens::ShowSettingsScreen(); }

// Without this, re-creating the status bar (e.g. on every automatic dark/light
// transition) would leak one StatusBarWidgets block and leave its RefreshTick timer
// running forever against an orphaned object.
void OnStatusBarDeleted(lv_event_t* e) {
    auto* bar_widgets = static_cast<StatusBarWidgets*>(lv_event_get_user_data(e));
    lv_timer_del(bar_widgets->refresh_timer);
    delete bar_widgets;
}

} // namespace

lv_obj_t* CreateStatusBar(lv_obj_t* parent) {
    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);

    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(bar, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    auto* bar_widgets = new StatusBarWidgets{};

    lv_obj_t* brand_label = lv_label_create(bar);
    lv_label_set_text(brand_label, "openWB");
    lv_obj_set_style_text_font(brand_label, &font_sans_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(brand_label, colors.muted, LV_PART_MAIN);

    // IGNORE_LAYOUT + center: brand_label and icons_row aren't the same width, so a
    // plain 3-child flex row wouldn't actually center this between them.
    bar_widgets->time_label = lv_label_create(bar);
    lv_obj_add_flag(bar_widgets->time_label, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_center(bar_widgets->time_label);
    lv_label_set_text(bar_widgets->time_label, "--:--");
    lv_obj_set_style_text_font(bar_widgets->time_label, &font_sans_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(bar_widgets->time_label, colors.text, LV_PART_MAIN);

    lv_obj_t* icons_row = CreateGroupRow(bar, LV_FLEX_ALIGN_START, 8);

    bar_widgets->issue_icon = CreateIconLabel(icons_row, icon::Glyph::kAlertTriangle, colors.warning,
                                               &font_icons_24);
    lv_obj_add_flag(bar_widgets->issue_icon, LV_OBJ_FLAG_HIDDEN);

    bar_widgets->update_icon = CreateIconLabel(icons_row, icon::Glyph::kCloudDownload, colors.accent,
                                                &font_icons_24);
    lv_obj_add_flag(bar_widgets->update_icon, LV_OBJ_FLAG_HIDDEN);

    bar_widgets->connection_icon = CreateIconLabel(icons_row, icon::Glyph::kWifi, colors.muted,
                                                    &font_icons_24);

    lv_obj_t* settings_icon = CreateIconLabel(icons_row, icon::Glyph::kSettings, colors.muted,
                                               &font_icons_24);
    lv_obj_add_flag(settings_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(settings_icon, 2, LV_PART_MAIN);
    lv_obj_add_event_cb(settings_icon, OnSettingsTapped, LV_EVENT_CLICKED, nullptr);

    Refresh(bar_widgets); // not just on the first timer tick, to avoid a stale placeholder
    bar_widgets->refresh_timer = lv_timer_create(RefreshTick, kRefreshIntervalMs, bar_widgets);
    lv_obj_add_event_cb(bar, OnStatusBarDeleted, LV_EVENT_DELETE, bar_widgets);

    return bar;
}

} // namespace widgets
