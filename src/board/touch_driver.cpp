#include "touch_driver.h"

#include <Arduino.h>
#include <lvgl.h>

#include "state/app_state.h"

namespace board {
namespace {

bool g_touch_event_hooked = false;

void OnIndevEvent(lv_event_t* /*e*/) {
    state::State().NoteTouch(millis());
}

void EnsureTouchEventHooked() {
    if (g_touch_event_hooked) return;
    for (lv_indev_t* indev = lv_indev_get_next(nullptr); indev != nullptr;
         indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_add_event_cb(indev, OnIndevEvent, LV_EVENT_PRESSED, nullptr);
            lv_indev_add_event_cb(indev, OnIndevEvent, LV_EVENT_PRESSING, nullptr);
            // Default (LV_DEF_REFR_PERIOD, 33ms) samples touch noticeably slower than
            // our own 5ms render tick — tightening just this timer (not the shared
            // display-refresh one) makes a swipe start following the finger sooner.
            lv_timer_set_period(lv_indev_get_read_timer(indev), 10);
        }
    }
    g_touch_event_hooked = true;
}

} // namespace

void SetTouchRotation(uint16_t rotation_degrees) {
    lv_display_t* disp = lv_display_get_default();
    if (disp == nullptr) return;

    lv_display_rotation_t rotation = LV_DISPLAY_ROTATION_0;
    switch (rotation_degrees) {
        case 90: rotation = LV_DISPLAY_ROTATION_90; break;
        case 180: rotation = LV_DISPLAY_ROTATION_180; break;
        case 270: rotation = LV_DISPLAY_ROTATION_270; break;
        default: rotation = LV_DISPLAY_ROTATION_0; break;
    }
    lv_display_set_rotation(disp, rotation);
    EnsureTouchEventHooked();
}

} // namespace board
