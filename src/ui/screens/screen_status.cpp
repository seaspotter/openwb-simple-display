#include "screen_status.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "config/config_store.h"
#include "screen_chargepoint.h"
#include "state/app_state.h"
#include "ui/theme/fonts/fonts.h"
#include "ui/theme/theme_colors.h"
#include "ui/theme/theme_quartz.h"
#include "ui/widgets/flow_bar.h"
#include "ui/widgets/format_power.h"
#include "ui/widgets/gesture_pager.h"
#include "ui/pin_prompt.h"
#include "ui/widgets/status_bar.h"
#include "ui/wifi_reset_prompt.h"

namespace screens {
namespace {

constexpr uint32_t kRefreshIntervalMs = 500;
// Max pool of legend/bar slots each bar can show; how many are actually visible each
// tick depends on how many categories are currently non-zero (see FilterNonZero).
constexpr uint8_t kSupplySlots = 3; // PV, grid import, battery discharge
constexpr uint8_t kDemandSlots = 6; // chargepoints, battery charge, grid export,
                                     // Verbraucher, Hausverbrauch, Zähler

std::vector<lv_obj_t*> g_chargepoint_cards;
// Re-created every time ShowStatusScreen runs (initial boot transition, plus every
// automatic dark/light schedule flip); tracked so the previous instance can be stopped
// instead of leaking a duplicate RefreshTick running forever against an orphaned screen.
lv_timer_t* g_refresh_timer = nullptr;

// A fresh device defaults to chargepoint_ids={1,0,0,0} (see app_config.h), which almost
// never matches a real install — the cards just sit empty ("Kein Fahrzeug bekannt")
// with no indication why. If MQTT is connected but none of the configured IDs have ever
// produced valid data after a grace period, this shows a one-time hint pointing at the
// web UI instead of leaving the person to guess. Both reset in ShowStatusScreen so a
// reboot after fixing the IDs gets a fresh grace period and can hint again if still wrong.
constexpr uint32_t kNoDataHintGraceMs = 10000;
uint32_t g_status_screen_shown_at_ms = 0;
bool g_shown_no_data_hint = false;

// Two bars: where power comes from (PV/grid import), where it's going (chargepoints/
// battery/grid export/Verbraucher). Each bar is a fixed set of *slots*, not fixed
// categories — every tick, categories are sorted by magnitude (smallest left) and
// rebound to whichever slot they land in, rather than each slot owning one category.
lv_obj_t* g_supply_bar = nullptr;
lv_obj_t* g_supply_icon_labels[kSupplySlots] = {};
lv_obj_t* g_supply_value_labels[kSupplySlots] = {};

lv_obj_t* g_demand_bar = nullptr;
lv_obj_t* g_demand_icon_labels[kDemandSlots] = {};
lv_obj_t* g_demand_value_labels[kDemandSlots] = {};

// One flow-bar category's current identity + value, sorted into slots each tick.
struct FlowItem {
    icon::Glyph glyph;
    lv_color_t color;
    float magnitude;
};

// Drops near-zero categories entirely rather than showing a "0 W" entry/sliver —
// e.g. an unconfigured Zähler, or a battery that's neither charging nor discharging.
uint8_t FilterNonZero(const FlowItem* candidates, uint8_t candidate_count, FlowItem* out) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < candidate_count; ++i) {
        if (candidates[i].magnitude >= 1.0f || candidates[i].magnitude <= -1.0f) out[n++] = candidates[i];
    }
    return n;
}

// Ascending insertion sort — at most 6 items, not worth pulling in <algorithm> for.
void SortAscending(FlowItem* items, uint8_t count) {
    for (uint8_t i = 1; i < count; ++i) {
        FlowItem key = items[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && items[j].magnitude > key.magnitude) {
            items[j + 1] = items[j];
            --j;
        }
        items[j + 1] = key;
    }
}

// Icon + value legend entry; glyph is set later by ApplySortedSlots since slots don't
// own a fixed category.
void CreateLegendEntry(lv_obj_t* parent, lv_obj_t** out_icon_label, lv_obj_t** out_value_label) {
    lv_obj_t* entry = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(entry, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(entry, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(entry, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(entry, 3, LV_PART_MAIN);
    lv_obj_set_flex_flow(entry, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(entry, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(entry, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_obj_t* icon_label = lv_label_create(entry);
    lv_label_set_text(icon_label, "");
    lv_obj_set_style_text_font(icon_label, &font_icons_16, LV_PART_MAIN);

    lv_obj_t* value = lv_label_create(entry);
    lv_label_set_text(value, "-");
    lv_obj_set_style_text_font(value, &font_sans_14, LV_PART_MAIN);

    *out_icon_label = icon_label;
    *out_value_label = value;
}

lv_obj_t* CreateLegendRow(lv_obj_t* parent) {
    lv_obj_t* legend = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(legend, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(legend, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(legend, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_row(legend, 2, LV_PART_MAIN);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW_WRAP); // 4 entries don't fit one line
    lv_obj_set_size(legend, lv_pct(100), LV_SIZE_CONTENT);
    return legend;
}

void OpenWifiResetPrompt(void* /*user_data*/) { ui::ShowWifiResetPrompt(); }
void OnLongPressWifiReset(lv_event_t* /*e*/) { ui::RequirePin(OpenWifiResetPrompt); }

// Only fires on the panel's own empty area, not its clickable children (mode switch,
// lock, stepper) — LVGL dispatches to the most specific clickable object under a touch.
void AttachWifiResetLongPress(lv_obj_t* obj) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, OnLongPressWifiReset, LV_EVENT_LONG_PRESSED, nullptr);
}

lv_obj_t* CreateGlobalPanel(lv_obj_t* parent) {
    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);

    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_add_style(panel, theme::PanelStyle(), LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 6, LV_PART_MAIN);
    lv_obj_set_size(panel, lv_pct(100), LV_SIZE_CONTENT);

    // Bar 1: where power currently comes from.
    lv_obj_t* supply_legend = CreateLegendRow(panel);
    for (uint8_t i = 0; i < kSupplySlots; ++i) {
        CreateLegendEntry(supply_legend, &g_supply_icon_labels[i], &g_supply_value_labels[i]);
    }

    lv_color_t placeholder_colors[widgets::kFlowBarMaxSegments];
    for (lv_color_t& c : placeholder_colors) c = colors.muted;
    g_supply_bar = widgets::CreateFlowBar(panel, placeholder_colors, kSupplySlots);
    // margin, not padding: pad_bottom shrinks the bar's own content height instead.
    lv_obj_set_style_margin_bottom(g_supply_bar, 4, LV_PART_MAIN);

    // Bar 2: where it's currently going.
    lv_obj_t* demand_legend = CreateLegendRow(panel);
    for (uint8_t i = 0; i < kDemandSlots; ++i) {
        CreateLegendEntry(demand_legend, &g_demand_icon_labels[i], &g_demand_value_labels[i]);
    }

    g_demand_bar = widgets::CreateFlowBar(panel, placeholder_colors, kDemandSlots);

    AttachWifiResetLongPress(panel);

    return panel;
}

// Sorts ascending by magnitude, then rebinds each active slot's icon/color/value and
// the matching flow-bar segment to its newly-assigned category. Slots beyond
// active_count (up to max_slots) are hidden entirely — see FilterNonZero.
void ApplySortedSlots(FlowItem* items, uint8_t active_count, uint8_t max_slots, lv_obj_t** icon_labels,
                       lv_obj_t** value_labels, lv_obj_t* bar) {
    SortAscending(items, active_count);

    float magnitudes[widgets::kFlowBarMaxSegments];
    lv_color_t bar_colors[widgets::kFlowBarMaxSegments];
    char buf[24];

    for (uint8_t i = 0; i < active_count; ++i) {
        char utf8[4];
        icon::Utf8Encode(items[i].glyph, utf8);
        lv_label_set_text(icon_labels[i], utf8);
        lv_obj_set_style_text_color(icon_labels[i], items[i].color, LV_PART_MAIN);

        widgets::FormatPowerW(items[i].magnitude, buf, sizeof(buf));
        lv_label_set_text(value_labels[i], buf);
        lv_obj_remove_flag(lv_obj_get_parent(icon_labels[i]), LV_OBJ_FLAG_HIDDEN);

        magnitudes[i] = items[i].magnitude;
        bar_colors[i] = items[i].color;
    }
    for (uint8_t i = active_count; i < max_slots; ++i) {
        lv_obj_add_flag(lv_obj_get_parent(icon_labels[i]), LV_OBJ_FLAG_HIDDEN);
    }
    widgets::UpdateFlowBar(bar, magnitudes, bar_colors, active_count);
}

void ShowNoChargepointDataHint() {
    const config::AppConfig& cfg = config::Store().Get();
    char ids_buf[32] = "";
    for (uint8_t i = 0; i < cfg.chargepoint_count && i < config::AppConfig::kMaxChargepoints; ++i) {
        char one[8];
        snprintf(one, sizeof(one), "%s%u", i == 0 ? "" : ", ", cfg.chargepoint_ids[i]);
        strlcat(ids_buf, one, sizeof(ids_buf));
    }

    lv_obj_t* mbox = lv_msgbox_create(nullptr);
    lv_obj_set_width(mbox, lv_pct(92));
    lv_obj_t* title = lv_msgbox_add_title(mbox, "Keine Ladepunkt-Daten");
    theme::ApplyMsgboxFont(title);
    lv_msgbox_add_close_button(mbox);
    lv_obj_t* text = lv_msgbox_add_text(
        mbox, "openWB ist verbunden, aber für die konfigurierte(n) Ladepunkt-ID(s) "
              "wurden noch keine Daten empfangen.");
    theme::ApplyMsgboxFont(text);

    lv_obj_t* content = lv_msgbox_get_content(mbox);
    char ids_line[64];
    snprintf(ids_line, sizeof(ids_line), "Konfiguriert: %s", ids_buf[0] != '\0' ? ids_buf : "-");
    lv_obj_t* ids_label = lv_label_create(content);
    lv_label_set_text(ids_label, ids_line);
    theme::ApplyMsgboxFont(ids_label);

    char url_line[80];
    snprintf(url_line, sizeof(url_line), "Bitte prüfen unter: http://%s.local", cfg.device_name);
    lv_obj_t* url_label = lv_label_create(content);
    lv_label_set_text(url_label, url_line);
    lv_label_set_long_mode(url_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(url_label, lv_pct(100));
    theme::ApplyMsgboxFont(url_label);
}

void RefreshTick(lv_timer_t* /*timer*/) {
    float chargepoints_total_w = 0;
    bool any_chargepoint_valid = false;
    for (uint8_t i = 0; i < g_chargepoint_cards.size(); ++i) {
        openwb::ChargepointStatus cp = state::State().GetChargepointStatus(i);
        RefreshChargepointCard(g_chargepoint_cards[i], cp);
        if (cp.valid) {
            chargepoints_total_w += cp.power_w;
            any_chargepoint_valid = true;
        }
    }

    if (!g_shown_no_data_hint && !any_chargepoint_valid && !g_chargepoint_cards.empty() &&
        state::State().IsMqttConnected() &&
        lv_tick_get() - g_status_screen_shown_at_ms >= kNoDataHintGraceMs) {
        g_shown_no_data_hint = true;
        ShowNoChargepointDataHint();
    }

    openwb::GlobalStatus global = state::State().GetGlobalStatus();
    if (!global.valid) return;

    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);

    // Grid/battery sign splits import/charging (demand) from export/discharging (supply).
    float grid_import_w = global.grid_power_w > 0 ? global.grid_power_w : 0;
    float grid_export_w = global.grid_power_w < 0 ? -global.grid_power_w : 0;
    float battery_charging_w = global.battery_power_w > 0 ? global.battery_power_w : 0;
    float battery_discharging_w = global.battery_power_w < 0 ? -global.battery_power_w : 0;

    if (g_supply_bar != nullptr) {
        FlowItem supply_candidates[kSupplySlots] = {
            {icon::Glyph::kSun, colors.pv, global.pv_power_w},
            {icon::Glyph::kBolt, colors.grid, grid_import_w},
            {icon::Glyph::kBattery, colors.battery, battery_discharging_w},
        };
        FlowItem supply_active[kSupplySlots];
        uint8_t supply_count = FilterNonZero(supply_candidates, kSupplySlots, supply_active);
        ApplySortedSlots(supply_active, supply_count, kSupplySlots, g_supply_icon_labels,
                          g_supply_value_labels, g_supply_bar);
    }

    if (g_demand_bar != nullptr) {
        FlowItem demand_candidates[kDemandSlots] = {
            {icon::Glyph::kBolt, colors.chargepoint, chargepoints_total_w},
            {icon::Glyph::kBattery, colors.battery, battery_charging_w},
            {icon::Glyph::kBolt, colors.grid, grid_export_w},
            {icon::Glyph::kDroplet, colors.consumer, global.consumer_power_w},
            {icon::Glyph::kHome, colors.muted, global.home_consumption_w},
            {icon::Glyph::kGauge, colors.grid, global.meter_power_w},
        };
        FlowItem demand_active[kDemandSlots];
        uint8_t demand_count = FilterNonZero(demand_candidates, kDemandSlots, demand_active);
        ApplySortedSlots(demand_active, demand_count, kDemandSlots, g_demand_icon_labels,
                          g_demand_value_labels, g_demand_bar);
    }
}

} // namespace

void ShowStatusScreen(uint8_t chargepoint_count) {
    // Captured before creating the new screen so it can be freed below instead of
    // leaking — this runs again on every automatic dark/light schedule flip, not just
    // once at boot.
    lv_obj_t* old_screen = lv_screen_active();

    g_status_screen_shown_at_ms = lv_tick_get();
    g_shown_no_data_hint = false;

    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_add_style(screen, theme::ScreenStyle(), LV_PART_MAIN);
    g_chargepoint_cards.clear();
    g_supply_bar = g_demand_bar = nullptr;
    for (uint8_t i = 0; i < kSupplySlots; ++i) g_supply_icon_labels[i] = g_supply_value_labels[i] = nullptr;
    for (uint8_t i = 0; i < kDemandSlots; ++i) g_demand_icon_labels[i] = g_demand_value_labels[i] = nullptr;

    if (chargepoint_count <= 1) {
        // No pager (only one card either way) — but the flow panel still shows if
        // enabled, same as with multiple chargepoints.
        lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(screen, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_row(screen, 8, LV_PART_MAIN);

        widgets::CreateStatusBar(screen);

        if (config::Store().Get().show_flow_chart) CreateGlobalPanel(screen);

        lv_obj_t* card = CreateChargepointCard(screen, /*chargepoint_id=*/
                                                config::Store().Get().chargepoint_ids[0]);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_flex_grow(card, 1);
        AttachWifiResetLongPress(card);
        g_chargepoint_cards.push_back(card);
    } else {
        lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(screen, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_row(screen, 12, LV_PART_MAIN);

        widgets::CreateStatusBar(screen);

        if (config::Store().Get().show_flow_chart) CreateGlobalPanel(screen);

        lv_obj_t* pager = widgets::CreateGesturePager(screen);
        lv_obj_set_width(pager, lv_pct(100));
        lv_obj_set_flex_grow(pager, 1);

        const config::AppConfig& cfg = config::Store().Get();
        for (uint8_t i = 0; i < chargepoint_count && i < config::AppConfig::kMaxChargepoints; ++i) {
            lv_obj_t* tile = widgets::CreatePagerTile(pager, i);
            g_chargepoint_cards.push_back(CreateChargepointCard(tile, cfg.chargepoint_ids[i]));
        }
    }

    lv_screen_load(screen);

    if (g_refresh_timer != nullptr) lv_timer_del(g_refresh_timer);
    g_refresh_timer = lv_timer_create(RefreshTick, kRefreshIntervalMs, nullptr);

    // Deleted last, after the new screen is active: frees the previous screen's whole
    // widget tree (status bar, mode switches, chargepoint cards and their heap state via
    // each widget's own LV_EVENT_DELETE handler) instead of leaking it.
    if (old_screen != nullptr && old_screen != screen) lv_obj_del(old_screen);
}

lv_obj_t* DebugGetFirstChargepointCard() {
    return g_chargepoint_cards.empty() ? nullptr : g_chargepoint_cards[0];
}

} // namespace screens
