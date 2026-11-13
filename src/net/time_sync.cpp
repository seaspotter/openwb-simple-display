#include "time_sync.h"

#include <Arduino.h>
#include <time.h>

namespace net {
namespace time_sync {

void Init() {
    // Europe/Berlin (CET/CEST), hardcoded to match this project's German-only UI.
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
}

} // namespace time_sync
} // namespace net
