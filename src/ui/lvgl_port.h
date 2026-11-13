// Owns the LVGL side of the boot sequence: display/touch bring-up (via board::InitDisplay)
// and a dedicated FreeRTOS task that drives LVGL's tick + timer handler and renders
// whichever screen is active. Runs pinned to core 1; src/net/net_task.* owns core 0, and
// the two only ever communicate through state::AppState (plan §5-§6).
#pragma once

namespace ui {

// Initializes the display/touch hardware, builds the initial screen (screen_boot), and
// starts the LVGL task. Call once from app_main()/setup().
void Init();

} // namespace ui
