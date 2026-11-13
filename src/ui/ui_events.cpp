#include "ui_events.h"

#include <lvgl.h>

#include "net/net_task.h"

namespace ui {
namespace {

constexpr uint32_t kDebounceMs = 400;
constexpr uint32_t kToastDurationMs = 2500;

uint8_t g_pending_cp_id = 0;
float g_pending_amps = 0;
bool g_pending_valid = false;
uint32_t g_pending_last_tap_ms = 0;

void PollCommandResults(lv_timer_t* /*timer*/) {
    state::CommandResult result;
    while (net::TryGetCommandResult(result)) {
        if (!result.success) ShowToast(result.message, /*is_error=*/true);
    }

    if (g_pending_valid && lv_tick_get() - g_pending_last_tap_ms >= kDebounceMs) {
        net::CommandRequest req{};
        req.type = net::CommandType::kSetChargecurrent;
        req.chargepoint_id = g_pending_cp_id;
        req.amps = g_pending_amps;
        net::EnqueueCommand(req);
        g_pending_valid = false;
    }
}

void HideToast(lv_timer_t* timer) {
    lv_obj_t* toast = static_cast<lv_obj_t*>(lv_timer_get_user_data(timer));
    lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_del(toast);
}

} // namespace

void InitEvents() { lv_timer_create(PollCommandResults, 100, nullptr); }

void ShowToast(const char* message, bool is_error) {
    lv_obj_t* screen = lv_screen_active();
    lv_obj_t* toast = lv_label_create(screen);
    lv_label_set_text(toast, message);
    lv_obj_set_style_text_color(toast, is_error ? lv_color_hex(0xe11d48) : lv_color_hex(0x1a1d24),
                                 LV_PART_MAIN);
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_timer_t* hide_timer = lv_timer_create(HideToast, kToastDurationMs, toast);
    lv_timer_set_repeat_count(hide_timer, 1);
}

void RequestChargecurrentDebounced(uint8_t chargepoint_id, float amps) {
    g_pending_cp_id = chargepoint_id;
    g_pending_amps = amps;
    g_pending_last_tap_ms = lv_tick_get();
    g_pending_valid = true;
}

} // namespace ui
