// WiFi reset is the one setting that can't be web-UI-only (per the user's preference,
// matching evcc-garage-display's pattern of putting settings behind the device's IP
// rather than an on-device menu): if WiFi itself is broken or misconfigured, the web UI
// is exactly what's unreachable, so resetting it needs an on-device trigger. Everything
// else (brightness, firmware version, OTA checks) lives in the web UI only — there's no
// on-device settings screen for those.
#pragma once

namespace ui {

// Shows a modal confirm dialog over whatever screen is currently active; on confirm,
// clears WiFi provisioning and reboots into the captive portal
// (net::ResetAndReprovision()). Wired to a long-press on the status screen's top panel
// (the global flow panel in multi-chargepoint mode, the chargepoint card itself in
// single-chargepoint mode — see screen_status.cpp) rather than a visible button, since
// this is a rare/recovery action, not a frequent one.
void ShowWifiResetPrompt();

} // namespace ui
