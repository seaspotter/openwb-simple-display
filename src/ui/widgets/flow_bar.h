// A single-snapshot, proportionally-segmented horizontal bar — the simplified quartz
// "Gesamt" energy-flow bar, showing relative current magnitudes rather than a
// time-series chart (no history data pipeline needed).
//
// Used twice on the status screen: one bar for where power currently comes from
// (PV + grid import), one for where it's currently going (chargepoints + battery
// charging + grid export) — quartz's own two-bar pattern, adapted to the fields this
// project actually tracks rather than its generic "Verbraucher"/"Zähler" categories,
// which aren't modeled here.
#pragma once

#include <cstdint>

#include <lvgl.h>

namespace widgets {

inline constexpr uint8_t kFlowBarMaxSegments = 4;

// Creates a bar with `count` colored segments (in the given order) under `parent`.
lv_obj_t* CreateFlowBar(lv_obj_t* parent, const lv_color_t* colors, uint8_t count);

// Resizes *and* recolors each segment (`count` must be <= what was passed to
// CreateFlowBar). Segments are positional slots, not fixed to one category — recoloring
// on every update is what lets the caller re-sort which category occupies which slot
// each tick (quartz orders its bar segments smallest-to-largest, left to right; this
// project's categories aren't in a fixed magnitude order, so the caller re-sorts and
// passes colors/magnitudes already in the desired slot order). Signs don't matter here —
// pass magnitudes that are already "only the part relevant to this bar" (e.g. grid import
// for a supply bar, 0 if currently exporting instead).
void UpdateFlowBar(lv_obj_t* bar, const float* magnitudes, const lv_color_t* colors, uint8_t count);

} // namespace widgets
