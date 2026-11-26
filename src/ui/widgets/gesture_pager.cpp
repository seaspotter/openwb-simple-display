#include "gesture_pager.h"

#include "config/config_store.h"
#include "ui/theme/theme_colors.h"
#include "ui/theme/theme_quartz.h"
#include "ui/widgets/layout_helpers.h"

namespace widgets {
namespace {

// LVGL's own scroll/snap animation time is a fixed 200-400ms (lv_obj_scroll_by, scaled
// off half the display's horizontal resolution) with no lv_conf.h knob to shorten it.
// LV_EVENT_SCROLL_BEGIN carries a pointer to that not-yet-started lv_anim_t, so this
// shortens it in place instead of patching the vendored library.
constexpr uint32_t kPageSwipeAnimMs = 80;

constexpr lv_coord_t kDotDiameter = 8;
constexpr lv_coord_t kDotGap = 8;

void OnPagerScrollBegin(lv_event_t* e) {
    auto* anim = static_cast<lv_anim_t*>(lv_event_get_param(e));
    if (anim != nullptr) lv_anim_set_duration(anim, kPageSwipeAnimMs);
}

// Tiles are added via lv_tileview_add_tile(pager, i, 0, ...) in strictly increasing i, so
// a tile's child index within the pager equals its page number — no separate bookkeeping
// of "which dot is this tile" needed.
void UpdateActiveDot(lv_obj_t* pager, lv_obj_t* dots) {
    lv_obj_t* active_tile = lv_tileview_get_tile_active(pager);
    uint32_t active_index = active_tile != nullptr ? lv_obj_get_index(active_tile) : 0;
    const theme::Palette& colors = theme::Colors(config::Store().Get().dark_mode);
    uint32_t count = lv_obj_get_child_count(dots);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* dot = lv_obj_get_child(dots, i);
        bool active = i == active_index;
        lv_obj_set_style_bg_color(dot, active ? colors.accent : colors.muted, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, active ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
    }
}

// lv_tileview only fires this once a swipe's snap animation has fully settled (see
// tileview_event_cb's LV_EVENT_SCROLL_END handling), so the dots update exactly when the
// new page becomes the active one, not mid-swipe.
void OnPagerValueChanged(lv_event_t* e) {
    auto* pager = static_cast<lv_obj_t*>(lv_event_get_target(e));
    auto* dots = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    UpdateActiveDot(pager, dots);
}

} // namespace

lv_obj_t* CreateGesturePager(lv_obj_t* parent) {
    lv_obj_t* pager = lv_tileview_create(parent);
    lv_obj_add_style(pager, theme::ScreenStyle(), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(pager, LV_SCROLLBAR_MODE_OFF); // replaced by CreatePageDots
    lv_obj_add_event_cb(pager, OnPagerScrollBegin, LV_EVENT_SCROLL_BEGIN, nullptr);
    return pager;
}

lv_obj_t* CreatePagerTile(lv_obj_t* pager, uint8_t index) {
    lv_obj_t* tile = lv_tileview_add_tile(pager, index, 0, LV_DIR_HOR); // horizontal-only

    lv_obj_add_style(tile, theme::ScreenStyle(), LV_PART_MAIN);
    return tile;
}

lv_obj_t* CreatePageDots(lv_obj_t* parent, lv_obj_t* pager) {
    lv_obj_t* dots = CreateGroupRow(parent, LV_FLEX_ALIGN_CENTER, kDotGap);
    lv_obj_set_width(dots, lv_pct(100));

    uint32_t count = lv_obj_get_child_count(pager);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* dot = lv_obj_create(dots);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, kDotDiameter, kDotDiameter);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }

    UpdateActiveDot(pager, dots);
    lv_obj_add_event_cb(pager, OnPagerValueChanged, LV_EVENT_VALUE_CHANGED, dots);
    return dots;
}

} // namespace widgets
