#include "gesture_pager.h"

#include "ui/theme/theme_quartz.h"

namespace widgets {

lv_obj_t* CreateGesturePager(lv_obj_t* parent) {
    lv_obj_t* pager = lv_tileview_create(parent);
    lv_obj_add_style(pager, theme::ScreenStyle(), LV_PART_MAIN);
    return pager;
}

lv_obj_t* CreatePagerTile(lv_obj_t* pager, uint8_t index) {
    lv_obj_t* tile = lv_tileview_add_tile(pager, index, 0, LV_DIR_HOR); // horizontal-only

    lv_obj_add_style(tile, theme::ScreenStyle(), LV_PART_MAIN);
    return tile;
}

} // namespace widgets
