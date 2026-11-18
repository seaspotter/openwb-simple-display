// On-device gate for the PIN already used by the web UI's HTTP Basic Auth (see
// api_routes.cpp's CheckAuth) — protects the settings screen and the WiFi-reset
// long-press the same way a PIN protects the web UI, so physical access to the display
// isn't an end-run around it.
#pragma once

namespace ui {

using PinSuccessCb = void (*)(void* user_data);

// Calls on_success(user_data) immediately if no PIN is configured (AppConfig::web_ui_pin
// empty, the default). Otherwise shows an on-screen numeric keypad; on_success is only
// called once the entered PIN matches. Cancelling calls nothing.
void RequirePin(PinSuccessCb on_success, void* user_data = nullptr);

} // namespace ui
