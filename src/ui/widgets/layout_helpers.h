#pragma once

#include <lvgl.h>

#include "ui/theme/fonts/fonts.h"

namespace widgets {

inline lv_obj_t* CreateIconLabel(lv_obj_t* parent, icon::Glyph glyph, lv_color_t color,
                                  const lv_font_t* font = &font_icons_16) {
    char utf8[4];
    icon::Utf8Encode(glyph, utf8);
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, utf8);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

// Transparent, borderless, content-sized flex row for grouping a few widgets inline.
inline lv_obj_t* CreateGroupRow(lv_obj_t* parent, lv_flex_align_t main_align, lv_coord_t pad_column = 6) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, pad_column, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

} // namespace widgets
