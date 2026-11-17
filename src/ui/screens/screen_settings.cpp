#include "screen_settings.h"

#include <cstring>

#include <lvgl.h>

#include "app_config_build.h"
#include "board/display_driver.h"
#include "config/config_store.h"
#include "ota/ota_manager.h"
#include "state/app_state.h"
#include "ui/theme/fonts/fonts.h"
#include "ui/theme/theme_colors.h"
#include "ui/theme/theme_quartz.h"
#include "ui/widgets/layout_helpers.h"
#include "ui/wifi_reset_prompt.h"

namespace screens {
namespace {

lv_obj_t* g_ota_status_label = nullptr;
lv_obj_t* g_install_button = nullptr;

constexpr int32_t kBrightnessMinPct = 10;
constexpr int32_t kBrightnessMaxPct = 100;
constexpr int32_t kBrightnessStepPct = 5;

struct BrightnessState {
    int32_t value_pct;
    lv_obj_t* value_label;
};

// +/- instead of a slider: the slider's continuous drag was the thing producing the
// backlight artifact (reported as "brightness glitch"); discrete steps, same debounced-
// NVS-save pattern as the chargepoint current stepper, don't trigger it.
void OnBrightnessStep(lv_event_t* e) {
    auto* state = static_cast<BrightnessState*>(lv_event_get_user_data(e));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    intptr_t direction = reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn));

    state->value_pct += static_cast<int32_t>(direction) * kBrightnessStepPct;
    if (state->value_pct < kBrightnessMinPct) state->value_pct = kBrightnessMinPct;
    if (state->value_pct > kBrightnessMaxPct) state->value_pct = kBrightnessMaxPct;

    lv_label_set_text_fmt(state->value_label, "%d %%", static_cast<int>(state->value_pct));
    board::SetBacklight(static_cast<uint8_t>(state->value_pct));
    // Debounced, not written here directly: the NVS write itself blocks for long enough
    // to glitch the display if done on this task, which shares a core with rendering.
    state::State().SetPendingBrightnessSave(static_cast<uint8_t>(state->value_pct), lv_tick_get());
}

void OnBackTapped(lv_event_t* e) {
    lv_obj_t* previous = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    lv_screen_load(previous);
}

void OnResetWifiTapped(lv_event_t* /*e*/) { ui::ShowWifiResetPrompt(); }

void DoInstall(bool force) {
    ota::InstallResult result = ota::StartInstall(force);

    if (result.started) {
        lv_label_set_text(g_ota_status_label, "Installation läuft — Neustart folgt.");
        return;
    }

    if (strcmp(result.reason, "charging_active") == 0) {
        lv_obj_t* mbox = lv_msgbox_create(nullptr);
        lv_obj_t* title = lv_msgbox_add_title(mbox, "Ladevorgang aktiv");
        theme::ApplyMsgboxFont(title);
        lv_obj_t* text =
            lv_msgbox_add_text(mbox, "Ein Ladepunkt lädt gerade. Update trotzdem installieren?");
        theme::ApplyMsgboxFont(text);
        lv_obj_t* confirm_btn = lv_msgbox_add_footer_button(mbox, "Installieren");
        theme::ApplyMsgboxFont(confirm_btn);
        lv_obj_t* cancel_btn = lv_msgbox_add_footer_button(mbox, "Abbrechen");
        theme::ApplyMsgboxFont(cancel_btn);
        lv_obj_add_event_cb(
            confirm_btn, [](lv_event_t*) { DoInstall(/*force=*/true); }, LV_EVENT_CLICKED, nullptr);
        return;
    }

    lv_label_set_text_fmt(g_ota_status_label, "Fehlgeschlagen: %s", result.reason);
}

void OnInstallUpdateTapped(lv_event_t* /*e*/) { DoInstall(/*force=*/false); }

// Quartz card: panel style, column flex, a small muted heading as the first child.
lv_obj_t* CreateCard(lv_obj_t* parent, const char* heading, const theme::Palette& colors) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, theme::PanelStyle(), LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 10, LV_PART_MAIN);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* heading_label = lv_label_create(card);
    lv_label_set_text(heading_label, heading);
    lv_obj_set_style_text_font(heading_label, &font_sans_11, LV_PART_MAIN);
    lv_obj_set_style_text_color(heading_label, colors.muted, LV_PART_MAIN);

    return card;
}

void OnCheckUpdateTapped(lv_event_t* /*e*/) {
    ota::UpdateCheckResult result = ota::CheckForUpdate(/*auto_triggered=*/false);
    if (!result.success) {
        lv_label_set_text_fmt(g_ota_status_label, "Prüfung fehlgeschlagen: %s", result.message);
        return;
    }
    if (result.update_available) {
        lv_label_set_text_fmt(g_ota_status_label, "Update verfügbar: %s", result.latest_version);
        lv_obj_remove_flag(g_install_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(g_ota_status_label, "Aktuell — kein Update verfügbar.");
        lv_obj_add_flag(g_install_button, LV_OBJ_FLAG_HIDDEN);
    }
}

} // namespace

void ShowSettingsScreen() {
    lv_obj_t* previous_screen = lv_screen_active();
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_add_style(screen, theme::ScreenStyle(), LV_PART_MAIN);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(screen, 16, LV_PART_MAIN);

    const config::AppConfig& cfg = config::Store().Get();
    const theme::Palette& colors = theme::Colors(cfg.dark_mode);

    lv_obj_t* header_row = widgets::CreateGroupRow(screen, LV_FLEX_ALIGN_START, 12);
    lv_obj_t* back_btn = lv_button_create(header_row);
    lv_obj_add_style(back_btn, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_label_set_text(lv_label_create(back_btn), "< Zurück");
    lv_obj_add_event_cb(back_btn, OnBackTapped, LV_EVENT_CLICKED, previous_screen);

    lv_obj_t* title_label = lv_label_create(header_row);
    lv_label_set_text(title_label, "Einstellungen");
    lv_obj_set_style_text_font(title_label, &font_sans_14_bold, LV_PART_MAIN);

    // Anzeige.
    lv_obj_t* display_card = CreateCard(screen, "ANZEIGE", colors);

    lv_obj_t* brightness_label = lv_label_create(display_card);
    lv_label_set_text(brightness_label, "Helligkeit");

    auto* brightness_state = new BrightnessState{cfg.brightness_pct, nullptr};
    lv_obj_t* brightness_row = lv_obj_create(display_card);
    lv_obj_set_style_bg_opa(brightness_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(brightness_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(brightness_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(brightness_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(brightness_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_remove_flag(brightness_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* minus_btn = lv_button_create(brightness_row);
    lv_obj_add_style(minus_btn, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_obj_set_user_data(minus_btn, reinterpret_cast<void*>(-1));
    lv_label_set_text(lv_label_create(minus_btn), "-");
    lv_obj_add_event_cb(minus_btn, OnBrightnessStep, LV_EVENT_CLICKED, brightness_state);

    brightness_state->value_label = lv_label_create(brightness_row);
    lv_obj_set_style_pad_hor(brightness_state->value_label, 12, LV_PART_MAIN);
    lv_label_set_text_fmt(brightness_state->value_label, "%d %%", static_cast<int>(cfg.brightness_pct));

    lv_obj_t* plus_btn = lv_button_create(brightness_row);
    lv_obj_add_style(plus_btn, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_obj_set_user_data(plus_btn, reinterpret_cast<void*>(1));
    lv_label_set_text(lv_label_create(plus_btn), "+");
    lv_obj_add_event_cb(plus_btn, OnBrightnessStep, LV_EVENT_CLICKED, brightness_state);

    // Firmware.
    lv_obj_t* fw_card = CreateCard(screen, "FIRMWARE", colors);

    lv_obj_t* fw_row = lv_obj_create(fw_card);
    lv_obj_set_style_bg_opa(fw_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(fw_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(fw_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(fw_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fw_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(fw_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_remove_flag(fw_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_label_set_text(lv_label_create(fw_row), "Version");
    lv_obj_t* fw_value_label = lv_label_create(fw_row);
    lv_label_set_text(fw_value_label, FW_VERSION);
    lv_obj_set_style_text_font(fw_value_label, &font_sans_14_bold, LV_PART_MAIN);

    lv_obj_t* check_update_btn = lv_button_create(fw_card);
    lv_obj_add_style(check_update_btn, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_label_set_text(lv_label_create(check_update_btn), "Auf Update prüfen");
    lv_obj_add_event_cb(check_update_btn, OnCheckUpdateTapped, LV_EVENT_CLICKED, nullptr);

    g_install_button = lv_button_create(fw_card);
    lv_obj_add_style(g_install_button, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_label_set_text(lv_label_create(g_install_button), "Update installieren");
    lv_obj_add_flag(g_install_button, LV_OBJ_FLAG_HIDDEN); // shown once a check finds one
    lv_obj_add_event_cb(g_install_button, OnInstallUpdateTapped, LV_EVENT_CLICKED, nullptr);

    g_ota_status_label = lv_label_create(fw_card);
    lv_label_set_text(g_ota_status_label, "");
    lv_label_set_long_mode(g_ota_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_ota_status_label, lv_pct(100));

    // WLAN.
    lv_obj_t* wifi_card = CreateCard(screen, "WLAN", colors);

    lv_obj_t* reset_wifi_btn = lv_button_create(wifi_card);
    lv_obj_add_style(reset_wifi_btn, theme::FlatButtonStyle(), LV_PART_MAIN);
    lv_label_set_text(lv_label_create(reset_wifi_btn), "WLAN zurücksetzen");
    lv_obj_add_event_cb(reset_wifi_btn, OnResetWifiTapped, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* web_ui_label = lv_label_create(wifi_card);
    lv_label_set_text_fmt(web_ui_label, "Weitere Einstellungen: http://%s.local", cfg.device_name);
    lv_obj_set_style_text_color(web_ui_label, colors.muted, LV_PART_MAIN);
    lv_label_set_long_mode(web_ui_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(web_ui_label, lv_pct(100));

    lv_screen_load(screen);
}

} // namespace screens
