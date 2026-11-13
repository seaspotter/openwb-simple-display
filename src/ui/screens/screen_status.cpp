#include "screen_status.h"

#include <cstdio>
#include <vector>

#include "config/config_store.h"
#include "screen_chargepoint.h"
#include "state/app_state.h"
#include "ui/theme/fonts/fonts.h"
#include "ui/theme/theme_colors.h"
#include "ui/theme/theme_quartz.h"
#include "ui/widgets/flow_bar.h"
#include "ui/widgets/gesture_pager.h"
#include "ui/widgets/status_bar.h"
#include "ui/wifi_reset_prompt.h"

namespace screens {
namespace {

constexpr uint32_t kRefreshIntervalMs = 500;
constexpr uint8_t kSupplySlots = 2;
constexpr uint8_t kDemandSlots = 4;

std::vector<lv_obj_t*> g_chargepoint_cards;

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

// Ascending insertion sort — at most 4 items, not worth pulling in <algorithm> for.
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
    lv_obj_set_style_text_font(value, &font_sans_11, LV_PART_MAIN);

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

void OnLongPressWifiReset(lv_event_t* /*e*/) { ui::ShowWifiResetPrompt(); }

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

    const lv_color_t placeholder_colors[widgets::kFlowBarMaxSegments] = {
        colors.muted, colors.muted, colors.muted, colors.muted};
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

// Sorts ascending by magnitude, then rebinds each slot's icon/color/value and the
// matching flow-bar segment to its newly-assigned category.
void ApplySortedSlots(FlowItem* items, uint8_t count, lv_obj_t** icon_labels, lv_obj_t** value_labels,
                       lv_obj_t* bar) {
    SortAscending(items, count);

    float magnitudes[widgets::kFlowBarMaxSegments];
    lv_color_t bar_colors[widgets::kFlowBarMaxSegments];
    char buf[24];

    for (uint8_t i = 0; i < count; ++i) {
        char utf8[4];
        icon::Utf8Encode(items[i].glyph, utf8);
        lv_label_set_text(icon_labels[i], utf8);
        lv_obj_set_style_text_color(icon_labels[i], items[i].color, LV_PART_MAIN);

        snprintf(buf, sizeof(buf), "%.0f W", items[i].magnitude);
        lv_label_set_text(value_labels[i], buf);

        magnitudes[i] = items[i].magnitude;
        bar_colors[i] = items[i].color;
    }
    widgets::UpdateFlowBar(bar, magnitudes, bar_colors, count);
}

void RefreshTick(lv_timer_t* /*timer*/) {
    float chargepoints_total_w = 0;
    for (uint8_t i = 0; i < g_chargepoint_cards.size(); ++i) {
        openwb::ChargepointStatus cp = state::State().GetChargepointStatus(i);
        RefreshChargepointCard(g_chargepoint_cards[i], cp);
        if (cp.valid) chargepoints_total_w += cp.power_w;
    }

    openwb::GlobalStatus global = state::State().GetGlobalStatus();
    if (!global.valid) return;

    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);

    // Grid/battery sign splits import/charging (demand) from export/discharging (supply).
    float grid_import_w = global.grid_power_w > 0 ? global.grid_power_w : 0;
    float grid_export_w = global.grid_power_w < 0 ? -global.grid_power_w : 0;
    float battery_charging_w = global.battery_power_w > 0 ? global.battery_power_w : 0;

    if (g_supply_bar != nullptr) {
        FlowItem supply_items[kSupplySlots] = {
            {icon::Glyph::kSun, colors.pv, global.pv_power_w},
            {icon::Glyph::kPlugConnected, colors.grid, grid_import_w},
        };
        ApplySortedSlots(supply_items, kSupplySlots, g_supply_icon_labels, g_supply_value_labels,
                          g_supply_bar);
    }

    if (g_demand_bar != nullptr) {
        FlowItem demand_items[kDemandSlots] = {
            {icon::Glyph::kBolt, colors.chargepoint, chargepoints_total_w},
            {icon::Glyph::kBattery, colors.battery, battery_charging_w},
            {icon::Glyph::kPlugConnected, colors.grid, grid_export_w},
            {icon::Glyph::kHomeBolt, colors.consumer, global.consumer_power_w},
        };
        ApplySortedSlots(demand_items, kDemandSlots, g_demand_icon_labels, g_demand_value_labels,
                          g_demand_bar);
    }
}

} // namespace

void ShowStatusScreen(uint8_t chargepoint_count) {
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_add_style(screen, theme::ScreenStyle(), LV_PART_MAIN);
    g_chargepoint_cards.clear();
    g_supply_bar = g_demand_bar = nullptr;
    for (uint8_t i = 0; i < kSupplySlots; ++i) g_supply_icon_labels[i] = g_supply_value_labels[i] = nullptr;
    for (uint8_t i = 0; i < kDemandSlots; ++i) g_demand_icon_labels[i] = g_demand_value_labels[i] = nullptr;

    if (chargepoint_count <= 1) {
        // No pager, no global panel — just the status bar above the card.
        lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(screen, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_row(screen, 8, LV_PART_MAIN);

        widgets::CreateStatusBar(screen);

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

        CreateGlobalPanel(screen);

        lv_obj_t* pager = widgets::CreateGesturePager(screen);
        lv_obj_set_width(pager, lv_pct(100));
        lv_obj_set_flex_grow(pager, 1);

        const config::AppConfig& cfg = config::Store().Get();
        for (uint8_t i = 0; i < chargepoint_count; ++i) {
            lv_obj_t* tile = widgets::CreatePagerTile(pager, i);
            g_chargepoint_cards.push_back(CreateChargepointCard(tile, cfg.chargepoint_ids[i]));
        }
    }

    lv_screen_load(screen);
    lv_timer_create(RefreshTick, kRefreshIntervalMs, nullptr);
}

} // namespace screens
