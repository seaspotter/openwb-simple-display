// Matches quartz's own web formatting: plain watts below 1000, otherwise kW with a
// German decimal comma at 2 places.
#pragma once

#include <cstdio>

namespace widgets {

inline void FormatPowerW(float watts, char* out, size_t out_size) {
    float abs_w = watts < 0 ? -watts : watts;
    if (abs_w < 1000.0f) {
        snprintf(out, out_size, "%.0f W", watts);
        return;
    }
    snprintf(out, out_size, "%.2f kW", watts / 1000.0f);
    for (char* p = out; *p != '\0'; ++p) {
        if (*p == '.') {
            *p = ',';
            break;
        }
    }
}

} // namespace widgets
