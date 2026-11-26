#include <unity.h>

#include "util/sun_times.h"

using util::CalculateSunTimes;
using util::IsDarkByTimeRange;
using util::IsDarkBySunriseSunset;
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

// dark_start=22:00, light_start=07:00 — the normal wraps-past-midnight case.
void test_time_range_basic_boundaries() {
    TEST_ASSERT_FALSE(IsDarkByTimeRange(12, 0, 22, 0, 7, 0));  // midday: light
    TEST_ASSERT_TRUE(IsDarkByTimeRange(23, 0, 22, 0, 7, 0));   // late evening: dark
    TEST_ASSERT_TRUE(IsDarkByTimeRange(3, 0, 22, 0, 7, 0));    // middle of the night: dark
    TEST_ASSERT_TRUE(IsDarkByTimeRange(22, 0, 22, 0, 7, 0));   // exactly dark_start: dark
    TEST_ASSERT_FALSE(IsDarkByTimeRange(21, 59, 22, 0, 7, 0)); // just before dark_start: light
    TEST_ASSERT_FALSE(IsDarkByTimeRange(7, 0, 22, 0, 7, 0));   // exactly light_start: light
    TEST_ASSERT_TRUE(IsDarkByTimeRange(6, 59, 22, 0, 7, 0));   // just before light_start: dark
}

// Equal start/end is a degenerate zero-width "light" window, not a zero-width "dark"
// one — always dark, at any time of day, rather than silently never dark.
void test_equal_start_and_end_is_always_dark() {
    TEST_ASSERT_TRUE(IsDarkByTimeRange(0, 0, 22, 0, 22, 0));
    TEST_ASSERT_TRUE(IsDarkByTimeRange(12, 0, 22, 0, 22, 0));
    TEST_ASSERT_TRUE(IsDarkByTimeRange(22, 0, 22, 0, 22, 0));
}

void test_sunrise_sunset_midday_is_light_and_midnight_is_dark() {
    constexpr double kLat = 51.1657;
    constexpr double kLon = 10.4515;
    TEST_ASSERT_FALSE(IsDarkBySunriseSunset(12, 0, 172, kLat, kLon, 2.0)); // summer noon
    TEST_ASSERT_TRUE(IsDarkBySunriseSunset(1, 0, 172, kLat, kLon, 2.0));  // summer 1am
    TEST_ASSERT_FALSE(IsDarkBySunriseSunset(12, 0, 355, kLat, kLon, 1.0)); // winter noon
    TEST_ASSERT_TRUE(IsDarkBySunriseSunset(1, 0, 355, kLat, kLon, 1.0));  // winter 1am
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_summer_solstice_sunrise_and_sunset_are_in_plausible_range);
    RUN_TEST(test_winter_solstice_sunrise_and_sunset_are_in_plausible_range);
    RUN_TEST(test_summer_day_is_longer_than_winter_day);
    RUN_TEST(test_time_range_basic_boundaries);
    RUN_TEST(test_equal_start_and_end_is_always_dark);
    RUN_TEST(test_sunrise_sunset_midday_is_light_and_midnight_is_dark);
    return UNITY_END();
}
