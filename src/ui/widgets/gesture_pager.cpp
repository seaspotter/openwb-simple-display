#include "gesture_pager.h"

#include "ui/theme/theme_quartz.h"

namespace widgets {

lv_obj_t* CreateGesturePager(lv_obj_t* parent, uint8_t /*page_count*/) {
    lv_obj_t* pager = lv_tileview_create(parent);
    lv_obj_add_style(pager, theme::ScreenStyle(), LV_PART_MAIN); // flat, matches the
                                                                  // screen bg rather than
                                                                  // LVGL's default boxed
                                                                  // container look
    // Size left to the caller: when the parent is a flex container (screen_status.cpp
    // puts this below a global status strip), the caller sets width + flex_grow
    // instead, so this never fights that for the main-axis size.
    return pager;
}

lv_obj_t* CreatePagerTile(lv_obj_t* pager, uint8_t index) {
    // Horizontal-only paging: row fixed at 0, column = page index, LV_DIR_HOR.
    lv_obj_t* tile = lv_tileview_add_tile(pager, index, 0, LV_DIR_HOR);
    lv_obj_add_style(tile, theme::ScreenStyle(), LV_PART_MAIN);
    return tile;
}

} // namespace widgets
