// Quartz theme: LVGL default theme recolored to the quartz palette, plus shared styles
// screens/widgets apply explicitly via lv_obj_add_style(obj, theme::XStyle(), ...).
#pragma once

#include <lvgl.h>

namespace theme {

// (Re)initializes the theme and all styles below from the current palette. Safe to call
// again, e.g. on a dark-mode toggle.
void ApplyQuartzTheme(lv_display_t* display, bool dark_mode);

// Screen root background + default text color, no border.
const lv_style_t* ScreenStyle();

// Card/panel look: quartz panel bg + border, 14px radius, no shadow.
const lv_style_t* PanelStyle();

// Flat button/buttonmatrix look: 8px radius, no shadow, no border.
const lv_style_t* FlatButtonStyle();

// lv_msgbox's title/text/button labels don't inherit the theme's default font, so
// German umlauts render as tofu unless this is applied to each part explicitly.
void ApplyMsgboxFont(lv_obj_t* obj);

} // namespace theme
