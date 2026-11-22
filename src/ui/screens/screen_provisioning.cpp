#include "screen_provisioning.h"

#include <cstdio>
#include <cstring>

#include <lvgl.h>

#include "ui/theme/theme_quartz.h"

namespace screens {

void ShowProvisioningScreen(const char* ap_ssid, const char* ap_password) {
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_add_style(screen, theme::ScreenStyle(), LV_PART_MAIN);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(screen, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 16, LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Einrichtung erforderlich");

    lv_obj_t* instructions = lv_label_create(screen);
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(instructions, lv_pct(90));
    // A plain hyphen, not an em dash — this project's generated fonts (see
    // ui/theme/fonts/fonts.h) only cover Latin-1 for German umlauts, so U+2014 renders
    // as a tofu box (confirmed via the sim screenshot tool, same gotcha as pin_prompt.cpp's
    // backspace symbol).
    lv_label_set_text(instructions, "Dieses WLAN verbinden - per QR-Code oder manuell:");
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // Standard WiFi-QR payload (WIFI:T:<auth>;S:<ssid>;P:<password>;;), recognized by the
    // camera app on both Android and iOS without a dedicated scanner app. Neither ssid
    // nor password can contain a ';', ',', '"' or '\' that would need escaping here —
    // the ssid is always device_name + AP_SSID_SUFFIX and the password is always the
    // all-digit string from wifi_manager.cpp's GetApPassword().
    char qr_payload[128];
    snprintf(qr_payload, sizeof(qr_payload), "WIFI:T:WPA;S:%s;P:%s;;", ap_ssid, ap_password);
    lv_obj_t* qr = lv_qrcode_create(screen);
    lv_qrcode_set_size(qr, 200);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    // lv_qrcode_update(), not the newer lv_qrcode_set_data() string-convenience wrapper
    // — this project's pinned lvgl/lvgl@9.2.2 and the copy actually cached for the real
    // hardware env turned out to be inconsistent (9.2.2 vs 9.6.0, see platformio.ini's
    // own lib_deps comment and the fix that pins this explicitly), and lv_qrcode_update
    // is the one confirmed present in both.
    lv_qrcode_update(qr, qr_payload, strlen(qr_payload));

    lv_obj_t* creds = lv_label_create(screen);
    lv_label_set_long_mode(creds, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(creds, lv_pct(90));
    lv_label_set_text_fmt(creds, "WLAN: %s\nPasswort: %s", ap_ssid, ap_password);
    lv_obj_set_style_text_align(creds, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t* followup = lv_label_create(screen);
    lv_label_set_long_mode(followup, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(followup, lv_pct(90));
    lv_label_set_text(followup, "Anschließend den Anweisungen zum Abschluss der Einrichtung folgen.");
    lv_obj_set_style_text_align(followup, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_screen_load(screen);
}

} // namespace screens
