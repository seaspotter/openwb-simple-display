#include "flow_bar.h"

#include <cmath>

namespace widgets {
namespace {

// Scaled to a max of 24, not 100: an extreme flex_grow ratio (e.g. 100:1, which a
// near-zero segment next to a dominant one produces) reproducibly left the *whole bar*
// short of the container's right edge against the pinned lvgl@9.2.2 — confirmed by
// pixel-measuring a rendered screenshot, not a guess. A moderate max ratio (24:1) keeps
// visible proportionality while staying inside whatever range this flex_grow
// implementation handles cleanly.
uint8_t WeightFor(float magnitude, float total) {
    if (total < 1.0f) return 1;
    uint8_t weight = static_cast<uint8_t>(std::lround((magnitude / total) * 24.0f));
    return weight < 1 ? 1 : weight;
}

lv_obj_t* AddSegment(lv_obj_t* bar, lv_color_t color) {
    lv_obj_t* segment = lv_obj_create(bar);
    lv_obj_set_style_border_width(segment, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(segment, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(segment, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(segment, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_height(segment, lv_pct(100));
    lv_obj_set_width(segment, 0);
    lv_obj_set_flex_grow(segment, 1);
    lv_obj_remove_flag(segment, LV_OBJ_FLAG_SCROLLABLE);
    return segment;
}

} // namespace

lv_obj_t* CreateFlowBar(lv_obj_t* parent, const lv_color_t* colors, uint8_t count) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    // pad_all doesn't cover the gap *between* flex children — that's pad_column
    // specifically, and without zeroing it too, the default theme's flex gap left
    // visible gaps between segments that looked like background showing through.
    lv_obj_set_style_pad_column(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(bar, true, LV_PART_MAIN); // segments stay inside the
                                                             // pill-rounded ends
    lv_obj_set_size(bar, lv_pct(100), 8);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    if (count > kFlowBarMaxSegments) count = kFlowBarMaxSegments;
    for (uint8_t i = 0; i < count; ++i) AddSegment(bar, colors[i]);

    return bar;
}

void UpdateFlowBar(lv_obj_t* bar, const float* magnitudes, const lv_color_t* colors, uint8_t count) {
    if (count > kFlowBarMaxSegments) count = kFlowBarMaxSegments;

    float total = 0;
    for (uint8_t i = 0; i < count; ++i) total += std::fabs(magnitudes[i]);

    for (uint8_t i = 0; i < count; ++i) {
        lv_obj_t* segment = lv_obj_get_child(bar, i);
        lv_obj_set_flex_grow(segment, WeightFor(std::fabs(magnitudes[i]), total));
        lv_obj_set_style_bg_color(segment, colors[i], LV_PART_MAIN);
    }
}

} // namespace widgets
