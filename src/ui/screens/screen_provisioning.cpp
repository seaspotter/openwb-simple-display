#include "screen_provisioning.h"

#include <lvgl.h>

#include "ui/theme/theme_quartz.h"

namespace screens {

void ShowProvisioningScreen(const char* ap_ssid) {
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_add_style(screen, theme::ScreenStyle(), LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Einrichtung erforderlich");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    lv_obj_t* instructions = lv_label_create(screen);
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(instructions, lv_pct(80));
    lv_label_set_text_fmt(instructions,
                           "Mit dem Handy oder Computer dieses WLAN verbinden:\n\n%s\n\n"
                           "Anschließend den Anweisungen zum Abschluss der Einrichtung "
                           "folgen.",
                           ap_ssid);
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(instructions, LV_ALIGN_CENTER, 0, 0);

    lv_screen_load(screen);
}

} // namespace screens
