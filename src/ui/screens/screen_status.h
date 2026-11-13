// Home screen: single chargepoint full-screen, or a swipeable pager with a persistent
// status bar + flow panel when multiple chargepoints are configured.
#pragma once

#include <cstdint>

namespace screens {

void ShowStatusScreen(uint8_t chargepoint_count);

} // namespace screens
