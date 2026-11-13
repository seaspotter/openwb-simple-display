#include "mode_switch.h"

#include <cstdint>

#include "config/config_store.h"
#include "net/net_task.h"
#include "ui/theme/theme_colors.h"

namespace widgets {
namespace {

constexpr const char* kButtonMap[] = {"Sofort", "PV", "Ziel", "Eco", "Stopp", ""};

lv_style_t g_item_default_style; // unselected: no chrome at all, just text — matches
                                  // quartz's segmented-control look (confirmed against
                                  // the live theme: unselected options are plain text,
                                  // only the active one gets a filled pill)
lv_style_t g_item_checked_style; // selected: full pill radius; bg/text color come from
                                  // the base theme's own checked-state styling
                                  // (accent-colored), left untouched here

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

    uint8_t chargepoint_id = static_cast<uint8_t>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(obj)));

    net::CommandRequest req{};
    req.type = net::CommandType::kSetChargemode;
    req.chargepoint_id = chargepoint_id;
    req.mode = ModeForButtonIndex(selected);
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
    // Explicit, compact height: left unset, lv_buttonmatrix defaults to a much taller
    // box than its content needs, which made the selected item's pill (LV_RADIUS_CIRCLE
    // rounds to half of whichever is smaller of the button's width/height) balloon into
    // an oversized capsule instead of a tight oval hugging the text — confirmed against
    // a live screenshot, not a guess this time.
    lv_obj_set_size(matrix, lv_pct(100), 36);
    // Light "track" the pill slides within, matching the reference screenshot — the
    // page bg color reads as a visibly distinct track against the white card behind it.
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(matrix, colors.bg, LV_PART_MAIN);
    lv_obj_set_style_radius(matrix, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(matrix, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(matrix, 3, LV_PART_MAIN);
    // Quartz's segmented control: unselected options are plain text with zero button
    // chrome; only the selected one gets a filled, fully-rounded pill (confirmed
    // against the live theme's CSS and a screenshot) — not "every option is a button."
    lv_obj_add_style(matrix, &g_item_default_style, LV_PART_ITEMS);
    lv_obj_add_style(matrix, &g_item_checked_style, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_buttonmatrix_set_map(matrix, kButtonMap);
    lv_buttonmatrix_set_one_checked(matrix, true);
    lv_obj_set_user_data(matrix, reinterpret_cast<void*>(static_cast<intptr_t>(chargepoint_id)));
    lv_obj_add_event_cb(matrix, OnValueChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    return matrix;
}

void SetSelectedMode(lv_obj_t* mode_switch, openwb::ChargeMode mode) {
    lv_buttonmatrix_set_button_ctrl(mode_switch, ButtonIndexForMode(mode), LV_BUTTONMATRIX_CTRL_CHECKED);
}

} // namespace widgets
