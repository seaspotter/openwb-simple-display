// Sunrise/sunset calculation (the standard Almanac algorithm), used to auto-switch the
// display's dark/light theme without needing an external API or user-entered location.
#pragma once

#include <cstdint>

namespace util {

struct SunTimes {
    double sunrise_hour = 6.0; // local decimal hours, e.g. 7.25 == 07:15
    double sunset_hour = 18.0;
};

// `day_of_year` is 1-365/366 (January 1st == 1). `utc_offset_hours` should include DST
// (e.g. +1 for CET, +2 for CEST) — on this project, read it from a TZ-configured
// struct tm's tm_gmtoff (see time_sync.cpp), not hardcoded, so DST is handled for free.
SunTimes CalculateSunTimes(int day_of_year, double lat_deg, double lon_deg, double utc_offset_hours);

// True if `current_hour`:`current_minute` falls inside the dark period that runs from
// dark_start to light_start — wrapping past midnight, as a real evening-to-morning dark
// period does (e.g. dark_start 22:00, light_start 07:00).
bool IsDarkByTimeRange(int current_hour, int current_minute, uint8_t dark_start_hour, uint8_t dark_start_min,
                       uint8_t light_start_hour, uint8_t light_start_min);

// True if the current local time falls outside [sunrise, sunset) for the given day.
bool IsDarkBySunriseSunset(int current_hour, int current_minute, int day_of_year, double lat_deg,
                            double lon_deg, double utc_offset_hours);

} // namespace util
