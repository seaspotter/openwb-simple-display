// Idle/charging gate for *automatic* OTA installs (plan §9). Manual installs triggered
// from the web UI or on-device settings screen bypass this by default (surfaced as an
// explicit confirm if charging is detected, not a silent override — see
// web/api_routes.cpp's /api/ota/start `force` flag).
#pragma once

namespace ota {

// True if no configured chargepoint is currently charging AND no touch has been
// registered in the last few minutes, per state::AppState. Automatic background OTA
// installs should only proceed when this is true; automatic *checks* (not installs)
// are never gated.
bool ShouldAttemptAutomaticInstall();

} // namespace ota
