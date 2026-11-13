// Display/touch bring-up and the LVGL render task. Runs pinned to core 1; net_task owns
// core 0, and the two only communicate through state::AppState.
#pragma once

namespace ui {

// Initializes display/touch, shows the boot screen, and starts the LVGL task.
void Init();

} // namespace ui
