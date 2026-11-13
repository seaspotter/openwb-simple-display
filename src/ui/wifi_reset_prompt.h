#pragma once

namespace ui {

// Shows a confirm dialog; on confirm, clears WiFi provisioning and reboots into the
// captive portal.
void ShowWifiResetPrompt();

} // namespace ui
