// Defaults themselves live as default member initializers on config::AppConfig (see
// app_config.h) so there's exactly one place that states them. This header just exposes
// a way to get a fresh default-valued instance, used by config_store.cpp on first boot
// and by ConfigStore::ResetToDefaults() (not currently exposed through any UI — a full
// config reset is more than what the on-device WiFi-reset prompt does, and isn't yet
// surfaced in the web UI either).
#pragma once

#include "app_config.h"

namespace config {

inline AppConfig DefaultConfig() { return AppConfig{}; }

} // namespace config
