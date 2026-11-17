#include "wifi_reset_prompt.h"

#include <lvgl.h>

#include "net/wifi_manager.h"
#include "ui/theme/theme_quartz.h"

namespace ui {
namespace {

void OnConfirmed(lv_event_t* /*e*/) { net::ResetAndReprovision(); }

// LVGL v9's lv_msgbox_add_footer_button() does not auto-dismiss the box on click —
// without this, "Abbrechen" would do nothing and the modal would stay up forever.
void OnCancel(lv_event_t* e) {
    lv_obj_t* mbox = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    lv_msgbox_close(mbox);
}

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
    lv_obj_add_event_cb(cancel_btn, OnCancel, LV_EVENT_CLICKED, mbox);
}

} // namespace ui
