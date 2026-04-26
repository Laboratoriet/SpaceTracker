#pragma once

// Minimal C++ port of suncalc.js (https://github.com/mourner/suncalc),
// covering only the entry points the daylight tracker needs:
//   - getPosition(unixTime, lat, lng)  → sun altitude (radians) + azimuth
//   - getTimes(unixTime, lat, lng)     → sunrise / sunset / solar noon /
//                                        dawn / dusk for the local day
//
// All lat/lng values in DEGREES. All time values are Unix timestamps (seconds).
// Return values for sunrise/sunset etc. are also Unix seconds. Returns NaN
// when the sun never rises or never sets at the given location/date.
//
// Original suncalc.js is MIT-licensed by Volodymyr Agafonkin.

#include <stdint.h>
#include <time.h>

struct SunPosition {
    double altitude; // radians, positive = above horizon
    double azimuth;  // radians, 0 = south, +CCW toward east
};

struct SunTimes {
    time_t sunrise;     // upper edge appears
    time_t sunset;      // upper edge disappears
    time_t sunriseEnd;  // bottom edge clears horizon
    time_t sunsetStart; // bottom edge touches horizon
    time_t dawn;        // civil dawn (-6° altitude)
    time_t dusk;        // civil dusk
    time_t solarNoon;   // sun is highest
    time_t nadir;       // sun is lowest (~midnight, opposite hemisphere)
    bool   sunriseValid;
    bool   sunsetValid;
};

SunPosition suncalc_getPosition(time_t when, double lat, double lng);
SunTimes    suncalc_getTimes(time_t when, double lat, double lng);
