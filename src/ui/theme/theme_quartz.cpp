#include "theme_quartz.h"

#include "fonts/fonts.h"
#include "theme_colors.h"

namespace theme {
namespace {
lv_style_t g_screen_style;
lv_style_t g_panel_style;
lv_style_t g_flat_button_style;
} // namespace

void ApplyQuartzTheme(lv_display_t* display, bool dark_mode) {
    const Palette& colors = Colors(dark_mode);

    // font_sans_14 (Public Sans, generated with Latin-1 Supplement glyphs) rather than
    // LVGL's built-in Montserrat fallback, which has no German umlaut glyphs at all.
    lv_theme_default_init(display, colors.accent, colors.muted, dark_mode, &font_sans_14);

    lv_style_init(&g_screen_style);
    lv_style_set_bg_color(&g_screen_style, colors.bg);
    lv_style_set_bg_opa(&g_screen_style, LV_OPA_COVER);
    lv_style_set_text_color(&g_screen_style, colors.text);
    lv_style_set_border_width(&g_screen_style, 0);

    lv_style_init(&g_panel_style);
    lv_style_set_radius(&g_panel_style, kRadiusLarge);
    lv_style_set_shadow_width(&g_panel_style, 0);
    lv_style_set_border_width(&g_panel_style, 1);
    lv_style_set_border_color(&g_panel_style, colors.border);
    lv_style_set_bg_color(&g_panel_style, colors.panel);
    lv_style_set_bg_opa(&g_panel_style, LV_OPA_COVER);
    lv_style_set_text_color(&g_panel_style, colors.text);

    lv_style_init(&g_flat_button_style);
    lv_style_set_radius(&g_flat_button_style, kRadiusSmall);
    lv_style_set_shadow_width(&g_flat_button_style, 0);
    lv_style_set_border_width(&g_flat_button_style, 0);
}

const lv_style_t* ScreenStyle() { return &g_screen_style; }
const lv_style_t* PanelStyle() { return &g_panel_style; }
const lv_style_t* FlatButtonStyle() { return &g_flat_button_style; }

} // namespace theme
