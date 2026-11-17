#include <unity.h>

#include "util/sun_times.h"

using util::CalculateSunTimes;
using util::SunTimes;

void setUp() {}
void tearDown() {}

// Middle-of-Germany coordinates used by the real feature (see app_config.h).
constexpr double kLat = 51.1657;
constexpr double kLon = 10.4515;

void test_summer_solstice_sunrise_and_sunset_are_in_plausible_range() {
    // June 21st, day 172 of a non-leap year, CEST (UTC+2). Real-world values for this
    // region are roughly 05:15 sunrise / 21:35 sunset — allow generous slack since this
    // is an approximation algorithm, not a precision ephemeris.
    SunTimes t = CalculateSunTimes(172, kLat, kLon, 2.0);
    TEST_ASSERT_TRUE(t.sunrise_hour > 4.5 && t.sunrise_hour < 6.0);
    TEST_ASSERT_TRUE(t.sunset_hour > 21.0 && t.sunset_hour < 22.5);
}

void test_winter_solstice_sunrise_and_sunset_are_in_plausible_range() {
    // December 21st, day 355, CET (UTC+1). Real-world values are roughly 08:15 / 16:15.
    SunTimes t = CalculateSunTimes(355, kLat, kLon, 1.0);
    TEST_ASSERT_TRUE(t.sunrise_hour > 7.5 && t.sunrise_hour < 9.0);
    TEST_ASSERT_TRUE(t.sunset_hour > 15.5 && t.sunset_hour < 17.0);
}

void test_summer_day_is_longer_than_winter_day() {
    SunTimes summer = CalculateSunTimes(172, kLat, kLon, 2.0);
    SunTimes winter = CalculateSunTimes(355, kLat, kLon, 1.0);
    double summer_len = summer.sunset_hour - summer.sunrise_hour;
    double winter_len = winter.sunset_hour - winter.sunrise_hour;
    TEST_ASSERT_TRUE(summer_len > winter_len);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_summer_solstice_sunrise_and_sunset_are_in_plausible_range);
    RUN_TEST(test_winter_solstice_sunrise_and_sunset_are_in_plausible_range);
    RUN_TEST(test_summer_day_is_longer_than_winter_day);
    return UNITY_END();
}
