// Idle/charging gate for automatic OTA installs. Manual installs can bypass it (with
// an explicit confirm) via the `force` flag on /api/ota/start.
#pragma once

namespace ota {

// True if no configured chargepoint is charging and no touch was registered recently.
bool ShouldAttemptAutomaticInstall();

} // namespace ota
