#include "time_sync.h"

#include <Arduino.h>
#include <time.h>

namespace net {
namespace time_sync {

void Init() {
    // Public NTP pool; openWB itself isn't a reliable time source to depend on here.
    configTzTime("UTC0", "pool.ntp.org", "time.google.com");
}

} // namespace time_sync
} // namespace net
