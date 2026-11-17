#include "mode_switch.h"

#include <cstdint>

#include "config/config_store.h"
#include "net/net_task.h"
#include "ui/theme/theme_colors.h"

namespace widgets {
namespace {

constexpr const char* kButtonMap[] = {"Sofort", "PV", "Ziel", "Eco", "Stopp", ""};
// Generous margin above the slowest observed MQTT round-trip through openWB's own
// control loop (confirmed up to ~5s) — long enough that a real echo always arrives
// well before this, but short enough that a lost/dropped command doesn't leave the
// switch stuck showing a selection that never actually took.
constexpr uint32_t kPendingModeTimeoutMs = 10000;

lv_style_t g_item_default_style; // unselected: plain text, no chrome
lv_style_t g_item_checked_style; // selected: full pill radius

// screen_status.cpp's RefreshTick calls SetSelectedMode every 500ms from AppState,
// which still reflects the old mode until openWB's own echo comes back — without this,
// that periodic refresh stomps the optimistic highlight almost immediately after a tap,
// and the switch visibly sits on the old mode for the whole round-trip instead of
// reflecting the user's tap right away.
struct ModeSwitchUserData {
    uint8_t chargepoint_id;
    openwb::ChargeMode pending_mode;
    bool has_pending;
    uint32_t pending_set_ms;
};

openwb::ChargeMode ModeForButtonIndex(uint32_t index) {
    switch (index) {
        case 0: return openwb::ChargeMode::kInstant;
        case 1: return openwb::ChargeMode::kPv;
        case 2: return openwb::ChargeMode::kTarget;
        case 3: return openwb::ChargeMode::kEco;
        default: return openwb::ChargeMode::kStop;
    }
}

uint32_t ButtonIndexForMode(openwb::ChargeMode mode) {
    switch (mode) {
        case openwb::ChargeMode::kInstant: return 0;
        case openwb::ChargeMode::kPv: return 1;
        case openwb::ChargeMode::kTarget: return 2;
        case openwb::ChargeMode::kEco: return 3;
        case openwb::ChargeMode::kStop: return 4;
    }
    return 4;
}

void OnValueChanged(lv_event_t* e) {
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint32_t selected = lv_buttonmatrix_get_selected_button(obj);
    if (selected == LV_BUTTONMATRIX_BUTTON_NONE) return;

    auto* data = static_cast<ModeSwitchUserData*>(lv_obj_get_user_data(obj));
    openwb::ChargeMode mode = ModeForButtonIndex(selected);
    data->pending_mode = mode;
    data->has_pending = true;
    data->pending_set_ms = lv_tick_get();

    net::CommandRequest req{};
    req.type = net::CommandType::kSetChargemode;
    req.chargepoint_id = data->chargepoint_id;
    req.mode = mode;
    net::EnqueueCommand(req);
}

void EnsureStylesInitialized() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    lv_style_init(&g_item_default_style);
    lv_style_set_bg_opa(&g_item_default_style, LV_OPA_TRANSP);
    lv_style_set_border_width(&g_item_default_style, 0);
    lv_style_set_shadow_width(&g_item_default_style, 0);

    lv_style_init(&g_item_checked_style);
    lv_style_set_radius(&g_item_checked_style, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&g_item_checked_style, 0);
    lv_style_set_shadow_width(&g_item_checked_style, 0);
}

} // namespace

lv_obj_t* CreateModeSwitch(lv_obj_t* parent, uint8_t chargepoint_id) {
    EnsureStylesInitialized();

    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);

    lv_obj_t* matrix = lv_buttonmatrix_create(parent);
    // Explicit height: lv_buttonmatrix's default is much taller than its content,
    // which balloons the selected pill (LV_RADIUS_CIRCLE rounds to half the smaller
    // dimension) into an oversized capsule.
    lv_obj_set_size(matrix, lv_pct(100), 36);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(matrix, colors.bg, LV_PART_MAIN);
    lv_obj_set_style_radius(matrix, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(matrix, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(matrix, 3, LV_PART_MAIN);
    lv_obj_add_style(matrix, &g_item_default_style, LV_PART_ITEMS);
    lv_obj_add_style(matrix, &g_item_checked_style, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_buttonmatrix_set_map(matrix, kButtonMap);
    lv_buttonmatrix_set_one_checked(matrix, true);
    lv_obj_set_user_data(matrix, new ModeSwitchUserData{chargepoint_id, openwb::ChargeMode::kStop, false, 0});
    lv_obj_add_event_cb(matrix, OnValueChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    return matrix;
}

void SetSelectedMode(lv_obj_t* mode_switch, openwb::ChargeMode mode) {
    auto* data = static_cast<ModeSwitchUserData*>(lv_obj_get_user_data(mode_switch));
    if (data->has_pending) {
        if (mode == data->pending_mode) {
            data->has_pending = false; // openWB's echo caught up — reconciled
        } else if (lv_tick_elaps(data->pending_set_ms) < kPendingModeTimeoutMs) {
            return; // still waiting for the echo; don't stomp the optimistic highlight
        } else {
            data->has_pending = false; // gave up; trust whatever status reports now
        }
    }
    lv_buttonmatrix_set_button_ctrl(mode_switch, ButtonIndexForMode(mode), LV_BUTTONMATRIX_CTRL_CHECKED);
}

} // namespace widgets
