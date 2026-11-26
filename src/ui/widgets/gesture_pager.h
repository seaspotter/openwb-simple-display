// lv_tileview-based horizontal swipe between chargepoint pages.
#pragma once

#include <cstdint>

#include <lvgl.h>

namespace widgets {

lv_obj_t* CreateGesturePager(lv_obj_t* parent);
lv_obj_t* CreatePagerTile(lv_obj_t* pager, uint8_t index);

// Small circular page indicators below the pager (replaces the default scrollbar, which
// CreateGesturePager turns off) — one dot per tile, the current one highlighted. Create
// after all of `pager`'s tiles exist, since it reads the tile count at creation time.
lv_obj_t* CreatePageDots(lv_obj_t* parent, lv_obj_t* pager);

} // namespace widgets
