// Light/dark palettes, taken directly from the quartz web theme (plan §6) so screen and
// widget code never hardcodes a hex value — reference `theme::Colors(dark_mode).accent`
// etc. instead, making a future re-theme a one-file change.
#pragma once

#include <lvgl.h>

namespace theme {

struct Palette {
    lv_color_t bg;
    lv_color_t panel;
    lv_color_t border;
    lv_color_t text;
    lv_color_t muted;
    lv_color_t accent;
    lv_color_t warning;
    lv_color_t error;

    // Semantic energy-flow colors.
    lv_color_t pv;
    lv_color_t grid;
    lv_color_t chargepoint;
    lv_color_t battery;
    lv_color_t car;
    lv_color_t consumer; // "Verbraucher" — other household consumers
};

// lv_color_make() is a plain inline C function, not a C++ constexpr one, so these
// palettes are runtime-initialized `const` globals rather than `constexpr` — still
// computed once at static-init time, just not usable in a constant-expression context.
inline const Palette kLightPalette = {
    .bg = lv_color_make(0xf1, 0xf2, 0xf4),
    .panel = lv_color_make(0xff, 0xff, 0xff),
    .border = lv_color_make(0xe3, 0xe5, 0xe9),
    .text = lv_color_make(0x1a, 0x1d, 0x24),
    .muted = lv_color_make(0x6b, 0x72, 0x80),
    .accent = lv_color_make(0x25, 0x63, 0xeb),
    .warning = lv_color_make(0xea, 0x58, 0x0c),
    .error = lv_color_make(0xe1, 0x1d, 0x48),
    .pv = lv_color_make(0x1f, 0x9d, 0x55),
    .grid = lv_color_make(0xdc, 0x26, 0x26),
    .chargepoint = lv_color_make(0x25, 0x63, 0xeb),
    .battery = lv_color_make(0xea, 0xb3, 0x08),
    .car = lv_color_make(0x0d, 0x94, 0x88),
    .consumer = lv_color_make(0x7c, 0x3a, 0xed), // confirmed exact match to quartz's own
                                                   // `--consumer` CSS custom property,
                                                   // pulled directly from its live bundle
};

inline const Palette kDarkPalette = {
    .bg = lv_color_make(0x0a, 0x0b, 0x0d),
    .panel = lv_color_make(0x1f, 0x23, 0x2b),
    .border = lv_color_make(0x34, 0x3a, 0x46),
    .text = lv_color_make(0xe7, 0xe9, 0xee),
    .muted = lv_color_make(0x9a, 0xa0, 0xac),
    .accent = lv_color_make(0x25, 0x63, 0xeb), // stays the same in dark mode
    .warning = lv_color_make(0xfb, 0x92, 0x3c),
    .error = lv_color_make(0xfb, 0x71, 0x85),
    .pv = lv_color_make(0x3e, 0xc7, 0x76),
    .grid = lv_color_make(0xf8, 0x71, 0x71),
    .chargepoint = lv_color_make(0x5b, 0x9b, 0xff),
    .battery = lv_color_make(0xfa, 0xcc, 0x15),
    .car = lv_color_make(0x2d, 0xd4, 0xbf),
    .consumer = lv_color_make(0xa7, 0x8b, 0xfa), // confirmed exact match to quartz's dark-
                                                   // mode `--consumer` value
};

inline const Palette& Colors(bool dark_mode) {
    return dark_mode ? kDarkPalette : kLightPalette;
}

// Flat-design constants shared by every widget style (plan §6): 8-14px radii, ~zero
// shadow, rather than per-widget magic numbers.
inline constexpr lv_coord_t kRadiusSmall = 8;
inline constexpr lv_coord_t kRadiusLarge = 14;

} // namespace theme
