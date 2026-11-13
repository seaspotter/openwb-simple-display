// SNTP time sync, needed for TLS certificate validation during OTA (plan §9) — a
// cert's validity window can't be checked without a roughly-correct system clock.
#pragma once

namespace net {
namespace time_sync {

void Init();

} // namespace time_sync
} // namespace net
