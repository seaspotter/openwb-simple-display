// lv_tileview-based horizontal swipe between chargepoint pages (plan §6). Only used
// when chargepoint_count > 1 — screen_status.cpp shows a single screen_chargepoint
// full-screen directly otherwise, skipping this entirely.
#pragma once

#include <cstdint>

#include <lvgl.h>

namespace widgets {

// Creates an lv_tileview under `parent` with `page_count` horizontally-swipeable tiles.
// Returns the tileview; use lv_tileview_add_tile()'s return value per page (obtained by
// calling CreatePagerTile()) as the parent to build each screen_chargepoint instance
// into.
lv_obj_t* CreateGesturePager(lv_obj_t* parent, uint8_t page_count);
lv_obj_t* CreatePagerTile(lv_obj_t* pager, uint8_t index);

} // namespace widgets
