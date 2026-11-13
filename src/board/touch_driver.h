// GT911 touch is initialized together with the display by board::InitDisplay(); this
// module only adds rotation-aware coordinate handling and touch-timestamp tracking.
#pragma once

#include <cstdint>

namespace board {

// Applies AppConfig::rotation to the registered input device. Call after InitDisplay().
void SetTouchRotation(uint16_t rotation_degrees);

} // namespace board
