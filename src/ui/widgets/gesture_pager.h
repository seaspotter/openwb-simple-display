// lv_tileview-based horizontal swipe between chargepoint pages.
#pragma once

#include <cstdint>

#include <lvgl.h>

namespace widgets {

lv_obj_t* CreateGesturePager(lv_obj_t* parent);
lv_obj_t* CreatePagerTile(lv_obj_t* pager, uint8_t index);

} // namespace widgets
