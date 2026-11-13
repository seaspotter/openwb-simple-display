// Home screen (plan §6): single chargepoint shown full-screen, or a swipeable pager of
// screen_chargepoint cards with a persistent global status strip (PV/grid/battery) that
// stays visible across swipes, when multiple chargepoints are configured.
#pragma once

#include <cstdint>

namespace screens {

// Builds and loads the status screen for `chargepoint_count` configured chargepoints.
// Starts the lv_timer that refreshes every card + the global strip from state::State()
// on each tick — this is the one place the UI task reads network-task-written state.
void ShowStatusScreen(uint8_t chargepoint_count);

} // namespace screens
