// Applies LVGL's built-in default theme, recolored to the quartz palette, plus a small
// set of shared styles that actually give screens/widgets the quartz look (flat panels,
// 8-14px radii, no shadows, exact palette hex values) — plan §6.
//
// lv_theme_default_init() alone only recolors a few theme-driven bits (checked/primary
// button color); it does NOT give screens the quartz background/panel/border colors or
// remove LVGL's default drop shadows. That needs applying explicitly per object via the
// styles below — screens/widgets call lv_obj_add_style(obj, theme::XStyle(), LV_PART_MAIN)
// themselves rather than this module trying to auto-apply to every object (an earlier
// attempt at a second chained lv_theme_t, which fires for every object including label/
// button sub-parts, was both incompatible with the pinned lvgl@9.2.2 — no
// lv_theme_create() in this version's public API — and the wrong shape: it would have
// put a panel background behind things that should stay plain, like individual labels).
#pragma once

#include <lvgl.h>

namespace theme {

// Sets up LVGL's default theme with the quartz accent/muted colors, and (re)initializes
// all the styles below from the current palette. Safe to call again (e.g. when the user
// toggles dark mode) — re-initializes from scratch rather than diffing.
void ApplyQuartzTheme(lv_display_t* display, bool dark_mode);

// Screen root background: exact quartz bg color + default text color, no border. Call
// on every screen's root object right after creating it.
const lv_style_t* ScreenStyle();

// Card/container look: quartz panel bg + border, 14px radius, no shadow. Call on things
// that should read as a distinct panel (the chargepoint card, status tiles, settings
// sections) — not on every object.
const lv_style_t* PanelStyle();

// Flat button/buttonmatrix look: 8px radius, no shadow, no border. Leaves bg/text color
// alone (those come from the default theme's accent coloring, or are set explicitly per
// call site) — just strips LVGL's default rounder/shadowed button chrome.
const lv_style_t* FlatButtonStyle();

} // namespace theme
