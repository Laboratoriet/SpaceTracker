#pragma once

#include <Arduino.h>
#include "api.h"

// Satellite position
struct SatPos {
    double lat;     // degrees, -90 to 90
    double lon;     // degrees, -180 to 180
    double alt;     // km
    bool valid;
};

// Ground track: array of lat/lon points
#define MAX_TRACK_POINTS 180  // ~1.5 orbits at 1-min intervals

struct GroundTrack {
    float lats[MAX_TRACK_POINTS];
    float lons[MAX_TRACK_POINTS];
    int count;
};

// Satellite state
struct SatState {
    SatPos current;
    GroundTrack track;
    bool tleLoaded;
};

// Initialize SGP4 from TLE data
bool orbit_initSat(int satIndex, const TLEData &tle);

// Get current position of satellite
bool orbit_getPosition(int satIndex, SatPos &pos);

// Compute ground track (~1.5 orbits forward and back from now)
bool orbit_computeTrack(int satIndex, GroundTrack &track);

// Sample the satellite's recent path (one full orbit ago → now), one point
// per `stepSec`. pts[0] is the current position, pts[N-1] is ~92 minutes ago.
// Used for the comet-trail orbit ring on the globe view. Caller-allocated
// buffer; returns the number of points actually written (≤ maxPts).
//
// Each point is (lat°, lng°, altKm).
struct OrbitTrailPoint {
    float lat;
    float lng;
    float alt;
};
int orbit_computeTrail(int satIndex,
                       OrbitTrailPoint* pts, int maxPts,
                       int durationSec = 92 * 60, int stepSec = 30);

// Must be called after NTP sync to set the time reference
void orbit_setTime();

// Satellite indices
#define SAT_ISS      0
#define SAT_TIANGONG 1
