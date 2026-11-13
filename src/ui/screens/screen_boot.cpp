#include "screen_boot.h"

#include <lvgl.h>

#include "app_config_build.h"
#include "ui/theme/fonts/fonts.h"
#include "ui/theme/theme_quartz.h"

namespace screens {

void ShowBootScreen() {
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_add_style(screen, theme::ScreenStyle(), LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, PROJECT_NAME);
    lv_obj_set_style_text_font(title, &font_sans_20, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t* status = lv_label_create(screen);
    lv_label_set_text(status, "Verbinde...");
    lv_obj_set_style_text_color(status, lv_color_hex(0x6b7280), LV_PART_MAIN);
    lv_obj_align_to(status, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    lv_screen_load(screen);
}

} // namespace screens
