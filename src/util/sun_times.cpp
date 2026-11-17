#include "sun_times.h"

#include <cmath>

namespace util {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

double SinDeg(double deg) { return std::sin(deg * kDegToRad); }
double CosDeg(double deg) { return std::cos(deg * kDegToRad); }
double TanDeg(double deg) { return std::tan(deg * kDegToRad); }
double AsinDeg(double x) { return std::asin(x) * kRadToDeg; }
double AcosDeg(double x) { return std::acos(x) * kRadToDeg; }
double AtanDeg(double x) { return std::atan(x) * kRadToDeg; }

double NormalizeDeg(double deg) {
    while (deg < 0) deg += 360.0;
    while (deg >= 360.0) deg -= 360.0;
    return deg;
}

double NormalizeHour(double hour) {
    while (hour < 0) hour += 24.0;
    while (hour >= 24.0) hour -= 24.0;
    return hour;
}

// The standard "Sunrise/Sunset Algorithm" from the Almanac for Computers (1990),
// Nautical Almanac Office — the same formula behind most lightweight sunrise/sunset
// implementations. `is_sunrise` selects which of the two mirrored halves to run.
double CalculateOneEvent(int day_of_year, double lat_deg, double lon_deg, double utc_offset_hours,
                          bool is_sunrise) {
    constexpr double kZenith = 90.833; // official zenith: 90deg + refraction + solar radius

    double lng_hour = lon_deg / 15.0;
    double t = is_sunrise ? day_of_year + ((6.0 - lng_hour) / 24.0) : day_of_year + ((18.0 - lng_hour) / 24.0);

    double mean_anomaly = (0.9856 * t) - 3.289;

    double true_longitude = mean_anomaly + (1.916 * SinDeg(mean_anomaly)) +
                             (0.020 * SinDeg(2 * mean_anomaly)) + 282.634;
    true_longitude = NormalizeDeg(true_longitude);

    double right_ascension = AtanDeg(0.91764 * TanDeg(true_longitude));
    right_ascension = NormalizeDeg(right_ascension);
    // Put RA in the same quadrant as L, since atan() alone loses that information.
    double l_quadrant = std::floor(true_longitude / 90.0) * 90.0;
    double ra_quadrant = std::floor(right_ascension / 90.0) * 90.0;
    right_ascension = (right_ascension + (l_quadrant - ra_quadrant)) / 15.0; // hours

    double sin_declination = 0.39782 * SinDeg(true_longitude);
    double cos_declination = CosDeg(AsinDeg(sin_declination));

    double cos_hour_angle = (CosDeg(kZenith) - (sin_declination * SinDeg(lat_deg))) /
                             (cos_declination * CosDeg(lat_deg));
    // Outside +-1 means the sun never rises/sets that day at this latitude (polar
    // day/night) — not reachable at Germany's latitude, but clamp defensively anyway.
    if (cos_hour_angle > 1.0) cos_hour_angle = 1.0;
    if (cos_hour_angle < -1.0) cos_hour_angle = -1.0;

    double hour_angle = is_sunrise ? 360.0 - AcosDeg(cos_hour_angle) : AcosDeg(cos_hour_angle);
    hour_angle /= 15.0; // hours

    double local_mean_time = hour_angle + right_ascension - (0.06571 * t) - 6.622;
    double utc_time = NormalizeHour(local_mean_time - lng_hour);
    return NormalizeHour(utc_time + utc_offset_hours);
}

} // namespace

SunTimes CalculateSunTimes(int day_of_year, double lat_deg, double lon_deg, double utc_offset_hours) {
    SunTimes result;
    result.sunrise_hour = CalculateOneEvent(day_of_year, lat_deg, lon_deg, utc_offset_hours, /*is_sunrise=*/true);
    result.sunset_hour = CalculateOneEvent(day_of_year, lat_deg, lon_deg, utc_offset_hours, /*is_sunrise=*/false);
    return result;
}

} // namespace util
