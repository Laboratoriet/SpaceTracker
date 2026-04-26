// Hand-port of the relevant subset of suncalc.js.
// Reference: https://github.com/mourner/suncalc/blob/master/suncalc.js
// All formulas mirror the upstream JS line-for-line. See file header in
// suncalc.h for license.

#include "suncalc.h"

#include <math.h>

namespace {

// ── Date/time helpers ──────────────────────────────────────────────────────
// suncalc.js works in Julian dates anchored on J2000 (2000-01-01 12:00 UTC).
// The JS day count is `date.valueOf() / 86400000 - 0.5 + 2440588 - 2451545`.
// In Unix-seconds form that simplifies to:
//   days = (unix / 86400) - 0.5 + 2440588 - 2451545
//        = (unix / 86400) - 10957.5
constexpr double DAY_SEC      = 86400.0;
constexpr double J1970        = 2440588.0;
constexpr double J2000        = 2451545.0;
constexpr double J0           = 0.0009;
constexpr double RAD          = M_PI / 180.0;
constexpr double EARTH_OBLIQ  = RAD * 23.4397; // obliquity of the Earth

inline double toDays(time_t t) {
    // Unix seconds → Julian days since J2000 (2000-01-01 12:00 UTC)
    // Equivalent to suncalc.js: date.valueOf()/dayMs - 0.5 + J1970 - J2000
    return (double)t / DAY_SEC - 0.5 + J1970 - J2000;
}

inline time_t fromJulian(double j) {
    // Inverse of toDays for an absolute Julian day number (e.g. solarTransitJ
    // returns J2000 + days). Suncalc.js: new Date((j + 0.5 - J1970) * dayMs).
    // Bug fix: must NOT add J2000 here — j is already absolute.
    return (time_t)((j + 0.5 - J1970) * DAY_SEC);
}

// ── General sun calculations ───────────────────────────────────────────────
inline double rightAscension(double l, double b) {
    return atan2(sin(l) * cos(EARTH_OBLIQ) - tan(b) * sin(EARTH_OBLIQ), cos(l));
}
inline double declination(double l, double b) {
    return asin(sin(b) * cos(EARTH_OBLIQ) + cos(b) * sin(EARTH_OBLIQ) * sin(l));
}
inline double azimuthFn(double H, double phi, double dec) {
    return atan2(sin(H), cos(H) * sin(phi) - tan(dec) * cos(phi));
}
inline double altitudeFn(double H, double phi, double dec) {
    return asin(sin(phi) * sin(dec) + cos(phi) * cos(dec) * cos(H));
}
inline double siderealTime(double d, double lw) {
    return RAD * (280.16 + 360.9856235 * d) - lw;
}

inline double solarMeanAnomaly(double d) {
    return RAD * (357.5291 + 0.98560028 * d);
}
inline double eclipticLongitude(double M) {
    double C = RAD * (1.9148 * sin(M) + 0.02 * sin(2.0 * M) + 0.0003 * sin(3.0 * M));
    double P = RAD * 102.9372;
    return M + C + P + M_PI;
}

struct SunCoords { double dec, ra; };
inline SunCoords sunCoords(double d) {
    double M = solarMeanAnomaly(d);
    double L = eclipticLongitude(M);
    return { declination(L, 0), rightAscension(L, 0) };
}

// ── Sunrise / sunset support ───────────────────────────────────────────────
inline double julianCycle(double d, double lw) {
    return round(d - J0 - lw / (2.0 * M_PI));
}
inline double approxTransit(double Ht, double lw, double n) {
    return J0 + (Ht + lw) / (2.0 * M_PI) + n;
}
inline double solarTransitJ(double ds, double M, double L) {
    return J2000 + ds + 0.0053 * sin(M) - 0.0069 * sin(2.0 * L);
}
inline double hourAngle(double h, double phi, double d) {
    double cosH = (sin(h) - sin(phi) * sin(d)) / (cos(phi) * cos(d));
    if (cosH > 1.0 || cosH < -1.0) return NAN;
    return acos(cosH);
}

// Returns [setTime, riseTime] in Julian days.
struct EventTimes { double rise; double set; bool valid; };
EventTimes getSetJ(double h, double lw, double phi, double dec,
                   double n, double M, double L) {
    EventTimes out;
    out.rise  = 0;
    out.set   = 0;
    out.valid = false;
    double w = hourAngle(h, phi, dec);
    if (isnan(w)) return out;
    out.set   = solarTransitJ(approxTransit( w, lw, n), M, L);
    out.rise  = solarTransitJ(approxTransit(-w, lw, n), M, L);
    out.valid = true;
    return out;
}

} // namespace

// ── Public API ────────────────────────────────────────────────────────────

SunPosition suncalc_getPosition(time_t when, double lat, double lng) {
    double lw  = RAD * -lng;
    double phi = RAD * lat;
    double d   = toDays(when);

    SunCoords c = sunCoords(d);
    double H = siderealTime(d, lw) - c.ra;

    SunPosition p;
    p.altitude = altitudeFn(H, phi, c.dec);
    p.azimuth  = azimuthFn (H, phi, c.dec);
    return p;
}

SunTimes suncalc_getTimes(time_t when, double lat, double lng) {
    double lw  = RAD * -lng;
    double phi = RAD * lat;
    double d   = toDays(when);

    double n  = julianCycle(d, lw);
    double ds = approxTransit(0, lw, n);
    double M  = solarMeanAnomaly(ds);
    double L  = eclipticLongitude(M);
    double dec = declination(L, 0);
    double Jnoon = solarTransitJ(ds, M, L);

    SunTimes t = {};
    t.solarNoon = fromJulian(Jnoon);
    t.nadir     = fromJulian(Jnoon - 0.5);

    // Sunrise/sunset (upper edge at -0.833° to account for refraction + radius)
    EventTimes srs = getSetJ(RAD * -0.833, lw, phi, dec, n, M, L);
    t.sunrise = srs.valid ? fromJulian(srs.rise) : 0;
    t.sunset  = srs.valid ? fromJulian(srs.set)  : 0;
    t.sunriseValid = srs.valid;
    t.sunsetValid  = srs.valid;

    EventTimes srEnd = getSetJ(RAD * -0.3, lw, phi, dec, n, M, L);
    t.sunriseEnd  = srEnd.valid ? fromJulian(srEnd.rise) : t.sunrise;
    t.sunsetStart = srEnd.valid ? fromJulian(srEnd.set)  : t.sunset;

    // Civil dawn / dusk (sun at -6°)
    EventTimes civil = getSetJ(RAD * -6.0, lw, phi, dec, n, M, L);
    t.dawn = civil.valid ? fromJulian(civil.rise) : t.sunrise;
    t.dusk = civil.valid ? fromJulian(civil.set)  : t.sunset;

    return t;
}
