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
    lv_obj_t* current_value_label; // lives inside the settings popup, created on demand

    lv_obj_t* stat_power_label;
    lv_obj_t* stat_phases_label;
    lv_obj_t* stat_current_label;
    lv_obj_t* stat_charged_label;
    lv_obj_t* stat_range_label;

    float current_amps = kCurrentMinAmps;
    bool locked = false;
    openwb::ChargeMode current_mode = openwb::ChargeMode::kStop;

    // Cached from the latest status, read by OnSettingsTapped when building the popup.
    char instant_limit_type[8] = "none";
    float instant_limit_soc = 0;
    float instant_limit_amount_kwh = 0;
    char pv_limit_type[8] = "none";
    float pv_limit_soc = 0;
    float pv_limit_amount_kwh = 0;
    float max_price_eco_ct = 0;
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

    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);
    char utf8[4];
    icon::Utf8Encode(widgets->locked ? icon::Glyph::kLock : icon::Glyph::kLockOpen, utf8);
    lv_label_set_text(widgets->lock_icon_label, utf8);
    lv_obj_set_style_text_color(widgets->lock_icon_label, widgets->locked ? colors.error : colors.pv,
                                  LV_PART_MAIN);
}

const char* ModeTitle(openwb::ChargeMode mode) {
    switch (mode) {
        case openwb::ChargeMode::kInstant: return "Sofortladen";
        case openwb::ChargeMode::kPv: return "PV-Laden";
        case openwb::ChargeMode::kTarget: return "Zielladen";
        case openwb::ChargeMode::kEco: return "Eco-Laden";
        case openwb::ChargeMode::kStop: return "Gestoppt";
    }
    return "Einstellungen";
}

// A muted "eyebrow" label matching CreateStatColumn's style, for a setting's name
// inside the popup.
lv_obj_t* CreateSettingLabel(lv_obj_t* parent, const char* text) {
    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &font_sans_11, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, colors.muted, LV_PART_MAIN);
    return label;
}

// A centered -/value/+ row; `on_step` receives the row's own user_data (set by the
// caller) and the clicked button's direction (-1/+1) via its own user_data.
lv_obj_t* CreateStepperRow(lv_obj_t* parent, lv_event_cb_t on_step, void* user_data,
                            lv_obj_t** out_value_label) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);

    lv_obj_t* minus_btn = lv_button_create(row);
    lv_obj_add_style(minus_btn, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_obj_set_user_data(minus_btn, reinterpret_cast<void*>(-1));
    lv_label_set_text(lv_label_create(minus_btn), "-");
    lv_obj_add_event_cb(minus_btn, on_step, LV_EVENT_CLICKED, user_data);

    *out_value_label = lv_label_create(row);
    lv_obj_set_style_pad_hor(*out_value_label, 12, LV_PART_MAIN);

    lv_obj_t* plus_btn = lv_button_create(row);
    lv_obj_add_style(plus_btn, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_obj_set_user_data(plus_btn, reinterpret_cast<void*>(1));
    lv_label_set_text(lv_label_create(plus_btn), "+");
    lv_obj_add_event_cb(plus_btn, on_step, LV_EVENT_CLICKED, user_data);

    return row;
}

constexpr const char* kLimitTypeButtonMap[] = {"Keine", "SoC", "Menge", ""};

// Sofortladen/PV-Laden's "Ladebegrenzung" (none/soc/amount) editor — same shape for
// both modes, just a different pair of simpleAPI topics (see SetInstantLimit*/SetPvLimit*
// in openwb_mqtt.h). Heap-allocated per popup-open, same lifetime pattern as
// ModeSwitchUserData: leaked when the popup (and its widgets) are destroyed, like every
// other per-screen widget struct in this project.
struct LimitEditorState {
    uint8_t chargepoint_id;
    bool is_pv;
    float soc_value;
    float amount_value;
    lv_obj_t* soc_row;
    lv_obj_t* amount_row;
    lv_obj_t* soc_value_label;
    lv_obj_t* amount_value_label;
};

void OnLimitSocStep(lv_event_t* e) {
    auto* state = static_cast<LimitEditorState*>(lv_event_get_user_data(e));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    intptr_t dir = reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn));

    state->soc_value += static_cast<float>(dir) * 5.0f;
    if (state->soc_value < 0) state->soc_value = 0;
    if (state->soc_value > 100) state->soc_value = 100;
    lv_label_set_text_fmt(state->soc_value_label, "%.0f %%", state->soc_value);

    net::CommandRequest req{};
    req.type = state->is_pv ? net::CommandType::kSetPvLimitSoc : net::CommandType::kSetInstantLimitSoc;
    req.chargepoint_id = state->chargepoint_id;
    req.value = state->soc_value;
    net::EnqueueCommand(req);
}

void OnLimitAmountStep(lv_event_t* e) {
    auto* state = static_cast<LimitEditorState*>(lv_event_get_user_data(e));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    intptr_t dir = reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn));

    state->amount_value += static_cast<float>(dir) * 1.0f;
    if (state->amount_value < 1) state->amount_value = 1;
    if (state->amount_value > 50) state->amount_value = 50;
    lv_label_set_text_fmt(state->amount_value_label, "%.0f kWh", state->amount_value);

    net::CommandRequest req{};
    req.type = state->is_pv ? net::CommandType::kSetPvLimitAmount : net::CommandType::kSetInstantLimitAmount;
    req.chargepoint_id = state->chargepoint_id;
    req.value = state->amount_value;
    net::EnqueueCommand(req);
}

void OnLimitTypeChanged(lv_event_t* e) {
    auto* state = static_cast<LimitEditorState*>(lv_event_get_user_data(e));
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint32_t selected = lv_buttonmatrix_get_selected_button(obj);
    if (selected == LV_BUTTONMATRIX_BUTTON_NONE) return;

    const char* type = selected == 0 ? "none" : selected == 1 ? "soc" : "amount";
    net::CommandRequest req{};
    req.type = state->is_pv ? net::CommandType::kSetPvLimitType : net::CommandType::kSetInstantLimitType;
    req.chargepoint_id = state->chargepoint_id;
    strlcpy(req.limit_type, type, sizeof(req.limit_type));
    net::EnqueueCommand(req);

    lv_obj_add_flag(state->soc_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(state->amount_row, LV_OBJ_FLAG_HIDDEN);
    if (selected == 1) lv_obj_remove_flag(state->soc_row, LV_OBJ_FLAG_HIDDEN);
    else if (selected == 2) lv_obj_remove_flag(state->amount_row, LV_OBJ_FLAG_HIDDEN);
}

void CreateLimitEditor(lv_obj_t* parent, uint8_t chargepoint_id, bool is_pv, const char* initial_type,
                        float initial_soc, float initial_amount) {
    auto* state = new LimitEditorState{chargepoint_id, is_pv,  initial_soc, initial_amount,
                                        nullptr,         nullptr, nullptr,     nullptr};

    CreateSettingLabel(parent, "Ladebegrenzung");

    lv_obj_t* matrix = lv_buttonmatrix_create(parent);
    lv_obj_set_size(matrix, lv_pct(100), 36);
    lv_buttonmatrix_set_map(matrix, kLimitTypeButtonMap);
    lv_buttonmatrix_set_one_checked(matrix, true);
    uint32_t initial_index = strcmp(initial_type, "soc") == 0 ? 1 : strcmp(initial_type, "amount") == 0 ? 2 : 0;
    lv_buttonmatrix_set_button_ctrl(matrix, initial_index, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_add_event_cb(matrix, OnLimitTypeChanged, LV_EVENT_VALUE_CHANGED, state);

    state->soc_row = CreateStepperRow(parent, OnLimitSocStep, state, &state->soc_value_label);
    lv_label_set_text_fmt(state->soc_value_label, "%.0f %%", state->soc_value);
    if (initial_index != 1) lv_obj_add_flag(state->soc_row, LV_OBJ_FLAG_HIDDEN);

    state->amount_row = CreateStepperRow(parent, OnLimitAmountStep, state, &state->amount_value_label);
    lv_label_set_text_fmt(state->amount_value_label, "%.0f kWh", state->amount_value);
    if (initial_index != 2) lv_obj_add_flag(state->amount_row, LV_OBJ_FLAG_HIDDEN);
}

struct PriceEditorState {
    uint8_t chargepoint_id;
    float price_ct;
    lv_obj_t* value_label;
};

void OnPriceStep(lv_event_t* e) {
    auto* state = static_cast<PriceEditorState*>(lv_event_get_user_data(e));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    intptr_t dir = reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn));

    state->price_ct += static_cast<float>(dir) * 0.5f;
    if (state->price_ct < 0) state->price_ct = 0;
    lv_label_set_text_fmt(state->value_label, "%.1f ct/kWh", state->price_ct);

    net::CommandRequest req{};
    req.type = net::CommandType::kSetMaxPriceEco;
    req.chargepoint_id = state->chargepoint_id;
    req.value = state->price_ct;
    net::EnqueueCommand(req);
}

// Sofortladen's current stepper and the per-mode limit/price editors, relocated here
// from the card body (they used to show inline, pushing everything else down).
void OnSettingsTapped(lv_event_t* e) {
    auto* widgets = static_cast<ChargepointCardWidgets*>(lv_event_get_user_data(e));

    lv_obj_t* mbox = lv_msgbox_create(nullptr);
    lv_obj_t* title = lv_msgbox_add_title(mbox, ModeTitle(widgets->current_mode));
    theme::ApplyMsgboxFont(title);
    lv_msgbox_add_close_button(mbox);
    lv_obj_t* content = lv_msgbox_get_content(mbox);

    switch (widgets->current_mode) {
        case openwb::ChargeMode::kInstant: {
            CreateSettingLabel(content, "Ladestrom");
            lv_obj_t* current_row =
                CreateStepperRow(content, OnCurrentStep, widgets, &widgets->current_value_label);
            lv_obj_set_style_pad_bottom(current_row, 8, LV_PART_MAIN);
            lv_label_set_text_fmt(widgets->current_value_label, "%.0f A", widgets->current_amps);

            CreateLimitEditor(content, widgets->chargepoint_id, /*is_pv=*/false, widgets->instant_limit_type,
                               widgets->instant_limit_soc, widgets->instant_limit_amount_kwh);
            break;
        }
        case openwb::ChargeMode::kPv: {
            CreateLimitEditor(content, widgets->chargepoint_id, /*is_pv=*/true, widgets->pv_limit_type,
                               widgets->pv_limit_soc, widgets->pv_limit_amount_kwh);
            break;
        }
        case openwb::ChargeMode::kEco: {
            auto* price_state = new PriceEditorState{widgets->chargepoint_id, widgets->max_price_eco_ct, nullptr};
            CreateSettingLabel(content, "Max. Preis");
            lv_obj_t* price_row = CreateStepperRow(content, OnPriceStep, price_state, &price_state->value_label);
            (void)price_row;
            lv_label_set_text_fmt(price_state->value_label, "%.1f ct/kWh", price_state->price_ct);
            break;
        }
        default: {
            lv_obj_t* text = lv_msgbox_add_text(mbox, "Keine Einstellungen für diesen Lademodus verfügbar.");
            theme::ApplyMsgboxFont(text);
            break;
        }
    }
}

} // namespace

lv_obj_t* CreateChargepointCard(lv_obj_t* parent, uint8_t chargepoint_id) {
    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, theme::PanelStyle(), LV_PART_MAIN);
    lv_obj_set_size(card, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_top(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 10, LV_PART_MAIN);

    auto* widgets = new ChargepointCardWidgets{};
    widgets->chargepoint_id = chargepoint_id;
    lv_obj_set_user_data(card, widgets);

    // Header: name (left); lock, settings and state badge (right).
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
    lv_obj_set_style_pad_all(widgets->lock_icon_label, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(widgets->lock_icon_label, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widgets->lock_icon_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(widgets->lock_icon_label, colors.bg, LV_PART_MAIN);
    lv_obj_add_event_cb(widgets->lock_icon_label, OnLockToggle, LV_EVENT_CLICKED, widgets);

    lv_obj_t* settings_icon_label =
        widgets::CreateIconLabel(header_right, icon::Glyph::kSettings, colors.muted, &font_icons_24);
    lv_obj_add_flag(settings_icon_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(settings_icon_label, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(settings_icon_label, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_icon_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_icon_label, colors.bg, LV_PART_MAIN);
    lv_obj_add_event_cb(settings_icon_label, OnSettingsTapped, LV_EVENT_CLICKED, widgets);

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
    // Reserves ~3 lines up front so the layout doesn't jump when a longer message arrives.
    lv_obj_set_style_min_height(status_box, 90, LV_PART_MAIN);
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
    lv_label_set_text(widgets->soc_label, "SoC: -");

    widgets->mode_switch = widgets::CreateModeSwitch(card, chargepoint_id);

    // Stats: Leistung, Phasen, Strom, Geladen, Reichweite.
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

    lv_obj_t* phases_col = CreateStatColumn(stats_row, "Phasen");
    widgets->stat_phases_label = lv_label_create(phases_col);
    lv_obj_set_style_text_font(widgets->stat_phases_label, &font_sans_14, LV_PART_MAIN);
    lv_label_set_text(widgets->stat_phases_label, "-");

    lv_obj_t* current_col = CreateStatColumn(stats_row, "Strom");
    widgets->stat_current_label = lv_label_create(current_col);
    lv_obj_set_style_text_font(widgets->stat_current_label, &font_sans_14, LV_PART_MAIN);
    lv_label_set_text(widgets->stat_current_label, "0 A");

    lv_obj_t* charged_col = CreateStatColumn(stats_row, "Geladen");
    widgets->stat_charged_label = lv_label_create(charged_col);
    lv_obj_set_style_text_font(widgets->stat_charged_label, &font_sans_14, LV_PART_MAIN);
    lv_label_set_text(widgets->stat_charged_label, "0,0 kWh");

    lv_obj_t* range_col = CreateStatColumn(stats_row, "Reichweite");
    widgets->stat_range_label = lv_label_create(range_col);
    lv_obj_set_style_text_font(widgets->stat_range_label, &font_sans_14, LV_PART_MAIN);
    lv_label_set_text(widgets->stat_range_label, "-");

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
    lv_obj_set_style_text_color(widgets->lock_icon_label, status.locked ? colors.error : colors.pv, LV_PART_MAIN);

    if (status.connected_vehicle_name[0] != '\0') {
        lv_label_set_text(widgets->vehicle_name_label, status.connected_vehicle_name);
    } else {
        lv_label_set_text(widgets->vehicle_name_label, "Kein Fahrzeug bekannt");
    }
    if (status.soc_percent >= 0) {
        lv_label_set_text_fmt(widgets->soc_label, "SoC: %.0f%%", status.soc_percent);
    } else {
        lv_label_set_text(widgets->soc_label, "SoC: -");
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
    if (status.range_km >= 0) {
        lv_label_set_text_fmt(widgets->stat_range_label, "+%.0f km", status.range_km);
    } else {
        lv_label_set_text(widgets->stat_range_label, "-");
    }

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

    strlcpy(widgets->instant_limit_type, status.instant_limit_type, sizeof(widgets->instant_limit_type));
    widgets->instant_limit_soc = status.instant_limit_soc;
    widgets->instant_limit_amount_kwh = status.instant_limit_amount_kwh;
    strlcpy(widgets->pv_limit_type, status.pv_limit_type, sizeof(widgets->pv_limit_type));
    widgets->pv_limit_soc = status.pv_limit_soc;
    widgets->pv_limit_amount_kwh = status.pv_limit_amount_kwh;
    widgets->max_price_eco_ct = status.max_price_eco_ct;

    if (mode_known) {
        widgets->current_mode = mode;
        widgets::SetSelectedMode(widgets->mode_switch, mode);
    }
}

} // namespace screens
