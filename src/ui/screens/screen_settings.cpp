#include "screen_settings.h"

#include <cstring>

#include <lvgl.h>

#include "app_config_build.h"
#include "board/display_driver.h"
#include "config/config_store.h"
#include "ota/ota_manager.h"
#include "ui/theme/fonts/fonts.h"
#include "ui/theme/theme_colors.h"
#include "ui/theme/theme_quartz.h"
#include "ui/widgets/layout_helpers.h"
#include "ui/wifi_reset_prompt.h"

namespace screens {
namespace {

lv_obj_t* g_ota_status_label = nullptr;
lv_obj_t* g_install_button = nullptr;

void OnBrightnessChanged(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int32_t value = lv_slider_get_value(slider);
    board::SetBacklight(static_cast<uint8_t>(value));
    config::Store().Update([value](config::AppConfig& c) { c.brightness_pct = static_cast<uint8_t>(value); });
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

    lv_obj_t* brightness_slider = lv_slider_create(display_card);
    lv_obj_set_width(brightness_slider, lv_pct(100));
    lv_slider_set_range(brightness_slider, 10, 100);
    lv_slider_set_value(brightness_slider, cfg.brightness_pct, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, OnBrightnessChanged, LV_EVENT_VALUE_CHANGED, nullptr);

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
