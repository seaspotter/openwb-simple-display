// Defaults live as member initializers on AppConfig; this just exposes a fresh instance.
#pragma once

#include "app_config.h"

namespace config {

inline AppConfig DefaultConfig() { return AppConfig{}; }

} // namespace config
