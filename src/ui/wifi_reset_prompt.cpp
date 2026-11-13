#include "wifi_reset_prompt.h"

#include <lvgl.h>

#include "net/wifi_manager.h"
#include "ui/theme/fonts/fonts.h"

namespace ui {
namespace {

void OnConfirmed(lv_event_t* /*e*/) { net::ResetAndReprovision(); }

} // namespace

void ShowWifiResetPrompt() {
    lv_obj_t* mbox = lv_msgbox_create(nullptr);

    // lv_msgbox's title/text/button labels don't pick up the theme's default font
    // (font_sans_14, set via lv_theme_default_init) the way this project's own widgets
    // do — confirmed empirically via sim_screenshot: German umlauts rendered as tofu
    // here even though they render fine everywhere else. Set it explicitly on each part
    // rather than relying on inheritance.
    lv_obj_t* title = lv_msgbox_add_title(mbox, "WLAN zurücksetzen?");
    lv_obj_set_style_text_font(title, &font_sans_14, LV_PART_MAIN);

    lv_obj_t* text = lv_msgbox_add_text(mbox, "Das Display startet neu in den Einrichtungsmodus.");
    lv_obj_set_style_text_font(text, &font_sans_14, LV_PART_MAIN);

    lv_obj_t* confirm_btn = lv_msgbox_add_footer_button(mbox, "Zurücksetzen");
    lv_obj_set_style_text_font(confirm_btn, &font_sans_14, LV_PART_MAIN);

    lv_obj_t* cancel_btn = lv_msgbox_add_footer_button(mbox, "Abbrechen");
    lv_obj_set_style_text_font(cancel_btn, &font_sans_14, LV_PART_MAIN);

    lv_obj_add_event_cb(confirm_btn, OnConfirmed, LV_EVENT_CLICKED, nullptr);
}

} // namespace ui
