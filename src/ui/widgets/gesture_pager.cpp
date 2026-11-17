#include "gesture_pager.h"

#include "ui/theme/theme_quartz.h"

namespace widgets {
namespace {

// LVGL's own scroll/snap animation time is a fixed 200-400ms (lv_obj_scroll_by, scaled
// off half the display's horizontal resolution) with no lv_conf.h knob to shorten it.
// LV_EVENT_SCROLL_BEGIN carries a pointer to that not-yet-started lv_anim_t, so this
// shortens it in place instead of patching the vendored library.
constexpr uint32_t kPageSwipeAnimMs = 80;

void OnPagerScrollBegin(lv_event_t* e) {
    auto* anim = static_cast<lv_anim_t*>(lv_event_get_param(e));
    if (anim != nullptr) lv_anim_set_duration(anim, kPageSwipeAnimMs);
}

} // namespace

lv_obj_t* CreateGesturePager(lv_obj_t* parent) {
    lv_obj_t* pager = lv_tileview_create(parent);
    lv_obj_add_style(pager, theme::ScreenStyle(), LV_PART_MAIN);
    lv_obj_add_event_cb(pager, OnPagerScrollBegin, LV_EVENT_SCROLL_BEGIN, nullptr);
    return pager;
}

lv_obj_t* CreatePagerTile(lv_obj_t* pager, uint8_t index) {
    lv_obj_t* tile = lv_tileview_add_tile(pager, index, 0, LV_DIR_HOR); // horizontal-only

    lv_obj_add_style(tile, theme::ScreenStyle(), LV_PART_MAIN);
    return tile;
}

} // namespace widgets
