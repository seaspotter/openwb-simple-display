#include "screen_chargepoint.h"

#include <cstring>

#include "config/config_store.h"
#include "net/net_task.h"
#include "ui/theme/fonts/fonts.h"
#include "ui/theme/theme_colors.h"
#include "ui/theme/theme_quartz.h"
#include "ui/ui_events.h"
#include "ui/widgets/format_power.h"
#include "ui/widgets/layout_helpers.h"
#include "ui/widgets/mode_switch.h"

namespace screens {
namespace {

constexpr float kCurrentStepAmps = 1.0f;
constexpr float kCurrentMinAmps = 6.0f;
constexpr float kCurrentMaxAmps = 32.0f;

struct ChargepointCardWidgets {
    uint8_t chargepoint_id;

    lv_obj_t* name_label;
    lv_obj_t* lock_icon_label;
    lv_obj_t* state_label;
    lv_obj_t* status_box_label;
    lv_obj_t* vehicle_name_label;
    lv_obj_t* soc_label;

    lv_obj_t* mode_switch;
    lv_obj_t* stepper_row; // only shown in Sofort/instant mode
    lv_obj_t* current_value_label;

    lv_obj_t* stat_power_label;
    lv_obj_t* stat_phases_label;
    lv_obj_t* stat_current_label;
    lv_obj_t* stat_charged_label;

    float current_amps = kCurrentMinAmps;
    bool locked = false;
};

ChargepointCardWidgets* WidgetsOf(lv_obj_t* card) {
    return static_cast<ChargepointCardWidgets*>(lv_obj_get_user_data(card));
}

void OnCurrentStep(lv_event_t* e) {
    auto* widgets = static_cast<ChargepointCardWidgets*>(lv_event_get_user_data(e));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    intptr_t direction = reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn));

    widgets->current_amps += static_cast<float>(direction) * kCurrentStepAmps;
    if (widgets->current_amps < kCurrentMinAmps) widgets->current_amps = kCurrentMinAmps;
    if (widgets->current_amps > kCurrentMaxAmps) widgets->current_amps = kCurrentMaxAmps;

    lv_label_set_text_fmt(widgets->current_value_label, "%.0f A", widgets->current_amps);
    ui::RequestChargecurrentDebounced(widgets->chargepoint_id, widgets->current_amps);
}

// Pill badge matching quartz's .badge style.
void ApplyBadgeStyle(lv_obj_t* label, lv_color_t color, bool tinted) {
    lv_obj_set_style_radius(label, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(label, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(label, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, tinted ? static_cast<lv_opa_t>(46) : LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(label, tinted ? color : theme::Colors(config::Store().Get().dark_mode).bg,
                               LV_PART_MAIN);
    lv_obj_set_style_text_color(label, tinted ? color : theme::Colors(config::Store().Get().dark_mode).muted,
                                 LV_PART_MAIN);
}

lv_obj_t* CreateStatColumn(lv_obj_t* parent, const char* label_text) {
    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);

    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(col, 2, LV_PART_MAIN);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* label = lv_label_create(col);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, &font_sans_11, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, colors.muted, LV_PART_MAIN);

    return col;
}

void OnLockToggle(lv_event_t* e) {
    auto* widgets = static_cast<ChargepointCardWidgets*>(lv_event_get_user_data(e));
    widgets->locked = !widgets->locked;

    net::CommandRequest req{};
    req.type = net::CommandType::kSetChargepointLock;
    req.chargepoint_id = widgets->chargepoint_id;
    req.locked = widgets->locked;
    net::EnqueueCommand(req);

    char utf8[4];
    icon::Utf8Encode(widgets->locked ? icon::Glyph::kLock : icon::Glyph::kLockOpen, utf8);
    lv_label_set_text(widgets->lock_icon_label, utf8);
}

} // namespace

lv_obj_t* CreateChargepointCard(lv_obj_t* parent, uint8_t chargepoint_id) {
    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, theme::PanelStyle(), LV_PART_MAIN);
    lv_obj_set_size(card, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 10, LV_PART_MAIN);

    auto* widgets = new ChargepointCardWidgets{};
    widgets->chargepoint_id = chargepoint_id;
    lv_obj_set_user_data(card, widgets);

    // Header: name (left); lock + state badge (right).
    lv_obj_t* header_row = lv_obj_create(card);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(header_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_remove_flag(header_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* header_left = widgets::CreateGroupRow(header_row, LV_FLEX_ALIGN_START);
    widgets::CreateIconLabel(header_left, icon::Glyph::kBolt, colors.chargepoint);
    widgets->name_label = lv_label_create(header_left);
    lv_label_set_text(widgets->name_label, "Ladepunkt");
    lv_obj_set_style_text_font(widgets->name_label, &font_sans_14_bold, LV_PART_MAIN);

    // START, not END: a content-sized flex container with END alignment clips its
    // first child on this LVGL version.
    lv_obj_t* header_right = widgets::CreateGroupRow(header_row, LV_FLEX_ALIGN_START);
    widgets->lock_icon_label =
        widgets::CreateIconLabel(header_right, icon::Glyph::kLockOpen, colors.muted, &font_icons_24);
    lv_obj_add_flag(widgets->lock_icon_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(widgets->lock_icon_label, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(widgets->lock_icon_label, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widgets->lock_icon_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(widgets->lock_icon_label, colors.bg, LV_PART_MAIN);
    lv_obj_add_event_cb(widgets->lock_icon_label, OnLockToggle, LV_EVENT_CLICKED, widgets);

    widgets->state_label = lv_label_create(header_right);
    lv_label_set_text(widgets->state_label, "-");
    lv_obj_set_style_text_font(widgets->state_label, &font_sans_11, LV_PART_MAIN);
    ApplyBadgeStyle(widgets->state_label, colors.muted, /*tinted=*/false);

    // openWB's own state_str message, verbatim, in a tinted box.
    lv_obj_t* status_box = lv_obj_create(card);
    lv_obj_set_style_radius(status_box, theme::kRadiusSmall, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(status_box, colors.accent, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(status_box, static_cast<lv_opa_t>(28), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(status_box, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(status_box, 16, LV_PART_MAIN);
    lv_obj_set_size(status_box, lv_pct(100), LV_SIZE_CONTENT);
    // Reserves ~2.5 lines up front so the layout doesn't jump when a longer message arrives.
    lv_obj_set_style_min_height(status_box, 78, LV_PART_MAIN);
    lv_obj_remove_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);

    widgets->status_box_label = lv_label_create(status_box);
    lv_label_set_long_mode(widgets->status_box_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(widgets->status_box_label, lv_pct(100));
    lv_obj_set_style_text_font(widgets->status_box_label, &font_sans_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(widgets->status_box_label, colors.text, LV_PART_MAIN);
    lv_label_set_text(widgets->status_box_label, "-");

    // Connected vehicle + SoC.
    lv_obj_t* vehicle_row = lv_obj_create(card);
    lv_obj_set_style_bg_opa(vehicle_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(vehicle_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(vehicle_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(vehicle_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vehicle_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(vehicle_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_remove_flag(vehicle_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* vehicle_left = widgets::CreateGroupRow(vehicle_row, LV_FLEX_ALIGN_START);
    widgets::CreateIconLabel(vehicle_left, icon::Glyph::kCar, colors.car);
    widgets->vehicle_name_label = lv_label_create(vehicle_left);
    lv_label_set_text(widgets->vehicle_name_label, "Kein Fahrzeug bekannt");

    widgets->soc_label = lv_label_create(vehicle_row);
    lv_obj_set_style_text_color(widgets->soc_label, colors.muted, LV_PART_MAIN);
    lv_label_set_text(widgets->soc_label, "");

    widgets->mode_switch = widgets::CreateModeSwitch(card, chargepoint_id);

    widgets->stepper_row = lv_obj_create(card);
    lv_obj_t* stepper_row = widgets->stepper_row;
    lv_obj_add_flag(stepper_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(stepper_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_flex_flow(stepper_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stepper_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(stepper_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(stepper_row, 0, LV_PART_MAIN);
    lv_obj_set_size(stepper_row, lv_pct(100), LV_SIZE_CONTENT);

    lv_obj_t* minus_btn = lv_button_create(stepper_row);
    lv_obj_add_style(minus_btn, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_obj_set_user_data(minus_btn, reinterpret_cast<void*>(-1));
    lv_label_set_text(lv_label_create(minus_btn), "-");
    lv_obj_add_event_cb(minus_btn, OnCurrentStep, LV_EVENT_CLICKED, widgets);

    widgets->current_value_label = lv_label_create(stepper_row);
    lv_label_set_text_fmt(widgets->current_value_label, "%.0f A", widgets->current_amps);
    lv_obj_set_style_pad_hor(widgets->current_value_label, 12, LV_PART_MAIN);

    lv_obj_t* plus_btn = lv_button_create(stepper_row);
    lv_obj_add_style(plus_btn, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_obj_set_user_data(plus_btn, reinterpret_cast<void*>(1));
    lv_label_set_text(lv_label_create(plus_btn), "+");
    lv_obj_add_event_cb(plus_btn, OnCurrentStep, LV_EVENT_CLICKED, widgets);

    // Stats: Leistung, Strom, Phasen, Geladen.
    lv_obj_t* stats_row = lv_obj_create(card);
    lv_obj_set_style_bg_opa(stats_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(stats_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(stats_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(stats_row, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(stats_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(stats_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_remove_flag(stats_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* power_col = CreateStatColumn(stats_row, "Leistung");
    widgets->stat_power_label = lv_label_create(power_col);
    lv_obj_set_style_text_font(widgets->stat_power_label, &font_sans_14, LV_PART_MAIN);
    lv_label_set_text(widgets->stat_power_label, "0 W");

    lv_obj_t* current_col = CreateStatColumn(stats_row, "Strom");
    widgets->stat_current_label = lv_label_create(current_col);
    lv_obj_set_style_text_font(widgets->stat_current_label, &font_sans_14, LV_PART_MAIN);
    lv_label_set_text(widgets->stat_current_label, "0 A");

    lv_obj_t* phases_col = CreateStatColumn(stats_row, "Phasen");
    widgets->stat_phases_label = lv_label_create(phases_col);
    lv_obj_set_style_text_font(widgets->stat_phases_label, &font_sans_14, LV_PART_MAIN);
    lv_label_set_text(widgets->stat_phases_label, "-");

    lv_obj_t* charged_col = CreateStatColumn(stats_row, "Geladen");
    widgets->stat_charged_label = lv_label_create(charged_col);
    lv_obj_set_style_text_font(widgets->stat_charged_label, &font_sans_14, LV_PART_MAIN);
    lv_label_set_text(widgets->stat_charged_label, "0,0 kWh");

    return card;
}

void RefreshChargepointCard(lv_obj_t* card, const openwb::ChargepointStatus& status) {
    auto* widgets = WidgetsOf(card);
    if (widgets == nullptr || !status.valid) return;

    lv_label_set_text(widgets->name_label, status.name[0] != '\0' ? status.name : "Ladepunkt");

    const char* state = status.charging ? "Lädt" : status.plugged ? "Angesteckt, wartet" : "Nicht angesteckt";
    lv_label_set_text(widgets->state_label, state);
    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);
    if (status.charging) {
        ApplyBadgeStyle(widgets->state_label, colors.pv, /*tinted=*/true);
    } else if (status.plugged) {
        ApplyBadgeStyle(widgets->state_label, colors.battery, /*tinted=*/true);
    } else {
        ApplyBadgeStyle(widgets->state_label, colors.muted, /*tinted=*/false);
    }

    lv_label_set_text(widgets->status_box_label, status.state_str[0] != '\0' ? status.state_str : state);

    widgets->locked = status.locked;
    char lock_utf8[4];
    icon::Utf8Encode(status.locked ? icon::Glyph::kLock : icon::Glyph::kLockOpen, lock_utf8);
    lv_label_set_text(widgets->lock_icon_label, lock_utf8);

    if (status.connected_vehicle_name[0] != '\0') {
        lv_label_set_text(widgets->vehicle_name_label, status.connected_vehicle_name);
    } else {
        lv_label_set_text(widgets->vehicle_name_label, "Kein Fahrzeug bekannt");
    }
    if (status.soc_percent >= 0 && status.range_km >= 0) {
        lv_label_set_text_fmt(widgets->soc_label, "%.0f%% · +%.0f km", status.soc_percent, status.range_km);
    } else if (status.soc_percent >= 0) {
        lv_label_set_text_fmt(widgets->soc_label, "%.0f%%", status.soc_percent);
    } else {
        lv_label_set_text(widgets->soc_label, "");
    }

    char power_buf[24];
    widgets::FormatPowerW(status.power_w, power_buf, sizeof(power_buf));
    lv_label_set_text(widgets->stat_power_label, power_buf);
    if (status.phases_in_use > 0) {
        lv_label_set_text_fmt(widgets->stat_phases_label, "%u~", status.phases_in_use);
    } else {
        lv_label_set_text(widgets->stat_phases_label, "-");
    }
    lv_label_set_text_fmt(widgets->stat_current_label, "%.0f A", status.evse_current_a);
    lv_label_set_text_fmt(widgets->stat_charged_label, "%.1f kWh", status.energy_charged_wh / 1000.0f);

    openwb::ChargeMode mode = openwb::ChargeMode::kStop;
    bool mode_known = true;
    if (strcmp(status.chargemode, "instant_charging") == 0) {
        mode = openwb::ChargeMode::kInstant;
    } else if (strcmp(status.chargemode, "pv_charging") == 0) {
        mode = openwb::ChargeMode::kPv;
    } else if (strcmp(status.chargemode, "scheduled_charging") == 0) {
        mode = openwb::ChargeMode::kTarget;
    } else if (strcmp(status.chargemode, "eco_charging") == 0) {
        mode = openwb::ChargeMode::kEco;
    } else if (strcmp(status.chargemode, "stop") == 0) {
        mode = openwb::ChargeMode::kStop;
    } else {
        mode_known = false;
    }

    if (mode_known) {
        widgets::SetSelectedMode(widgets->mode_switch, mode);
        if (openwb::ChargeModeHasManualCurrent(mode)) {
            lv_obj_remove_flag(widgets->stepper_row, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widgets->stepper_row, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

} // namespace screens
