#include "wifi_reset_prompt.h"

#include <lvgl.h>

#include "net/wifi_manager.h"
#include "ui/theme/theme_quartz.h"

namespace ui {
namespace {

void OnConfirmed(lv_event_t* /*e*/) { net::ResetAndReprovision(); }

} // namespace

void ShowWifiResetPrompt() {
    lv_obj_t* mbox = lv_msgbox_create(nullptr);

    lv_obj_t* title = lv_msgbox_add_title(mbox, "WLAN zurücksetzen?");
    theme::ApplyMsgboxFont(title);

    lv_obj_t* text = lv_msgbox_add_text(mbox, "Das Display startet neu in den Einrichtungsmodus.");
    theme::ApplyMsgboxFont(text);

    lv_obj_t* confirm_btn = lv_msgbox_add_footer_button(mbox, "Zurücksetzen");
    theme::ApplyMsgboxFont(confirm_btn);

    lv_obj_t* cancel_btn = lv_msgbox_add_footer_button(mbox, "Abbrechen");
    theme::ApplyMsgboxFont(cancel_btn);

    lv_obj_add_event_cb(confirm_btn, OnConfirmed, LV_EVENT_CLICKED, nullptr);
}

} // namespace ui
