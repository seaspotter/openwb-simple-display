// A proportionally-segmented horizontal bar showing relative current magnitudes.
#pragma once

#include <cstdint>

#include <lvgl.h>

namespace widgets {

inline constexpr uint8_t kFlowBarMaxSegments = 6;

// Creates a bar with `count` colored segments (in the given order) under `parent`.
lv_obj_t* CreateFlowBar(lv_obj_t* parent, const lv_color_t* colors, uint8_t count);

// Resizes and recolors the first `count` segments (<= what was passed to CreateFlowBar)
// — segments are positional slots, not fixed to a category, so the caller can re-sort
// which category occupies which slot each update. Pass magnitudes already reduced to
// "only the part relevant to this bar" (e.g. grid import for a supply bar, 0 if
// currently exporting); signs don't matter here. Any segment beyond `count` (i.e. a
// slot the caller isn't using this tick — a zero/absent category) collapses to zero
// width rather than showing a sliver.
void UpdateFlowBar(lv_obj_t* bar, const float* magnitudes, const lv_color_t* colors, uint8_t count);

} // namespace widgets
