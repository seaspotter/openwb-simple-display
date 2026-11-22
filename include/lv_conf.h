// Minimal LVGL v9 configuration override. LVGL's own lv_conf_internal.h supplies a
// default for anything not set here, so this file only needs to state what this project
// actually wants different from stock — not reproduce the full upstream template.
//
// Validate during M1 hardware bring-up: if colors appear byte-swapped on the ST7701S
// panel, esp32-smartdisplay's README may recommend an additional color-swap setting here
// (e.g. LV_COLOR_16_SWAP) — not set yet since it's hardware-dependent, not knowable from
// the library's documentation alone.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// main.cpp drives lv_tick_inc()/lv_timer_handler() from its own FreeRTOS task (plan §6);
// LVGL doesn't need to manage OS primitives itself.
#define LV_USE_OS LV_OS_NONE

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

// Widgets actually used by the UI screens/widgets described in plan §6. Enabled
// explicitly so a missing one is a clear compile/link error rather than a silent
// no-op at runtime.
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_BUTTONMATRIX 1 // mode_switch segmented control
#define LV_USE_TILEVIEW 1     // gesture_pager swipe-between-chargepoints
#define LV_USE_BAR 1
#define LV_USE_SLIDER 1
#define LV_USE_IMAGE 1
#define LV_USE_IMAGEBUTTON 1
#define LV_USE_SWITCH 1
#define LV_USE_QRCODE 1 // screen_provisioning: WiFi-join QR for the setup AP's password

// Generated project fonts live in src/ui/theme/fonts/ (tools/fonts/generate_fonts.sh) and
// are declared there with LV_FONT_DECLARE; only the one built-in size actually used as a
// fallback before those are generated needs enabling here.
#define LV_FONT_MONTSERRAT_14 1

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

// Default (10px) made the chargepoint swipe pager feel like it took a noticeable beat
// to start following the finger; lower = it starts moving right away.
#define LV_INDEV_DEF_SCROLL_LIMIT 3

#endif // LV_CONF_H
