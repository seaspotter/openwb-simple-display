// SNTP time sync — also needed for TLS certificate validity checks during OTA.
#pragma once

namespace net {
namespace time_sync {

void Init();

} // namespace time_sync
} // namespace net
