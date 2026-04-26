#include "orbit.h"
#include <Sgp4.h>
#include <time.h>

// Two satellites: ISS and Tiangong
static Sgp4 satellites[2];
static bool sat_valid[2] = {false, false};

bool orbit_initSat(int satIndex, const TLEData &tle) {
    if (satIndex < 0 || satIndex > 1) return false;
    if (!tle.valid) return false;

    char line1[130], line2[130];
    tle.line1.toCharArray(line1, sizeof(line1));
    tle.line2.toCharArray(line2, sizeof(line2));

    satellites[satIndex].init(tle.name.c_str(), line1, line2);

    sat_valid[satIndex] = true;
    Serial.printf("[ORBIT] Initialized sat %d: %s\n", satIndex, tle.name.c_str());
    return true;
}

bool orbit_getPosition(int satIndex, SatPos &pos) {
    if (satIndex < 0 || satIndex > 1 || !sat_valid[satIndex]) {
        pos.valid = false;
        return false;
    }

    time_t now;
    time(&now);
    unsigned long unixTime = (unsigned long)now;

    satellites[satIndex].findsat(unixTime);

    pos.lat = satellites[satIndex].satLat;
    pos.lon = satellites[satIndex].satLon;
    pos.alt = satellites[satIndex].satAlt;
    pos.valid = true;

    return true;
}

bool orbit_computeTrack(int satIndex, GroundTrack &track) {
    if (satIndex < 0 || satIndex > 1 || !sat_valid[satIndex]) {
        track.count = 0;
        return false;
    }

    time_t now;
    time(&now);

    // Compute ~1.5 orbits: 135 minutes total (ISS period ~92 min)
    // From -45 min to +90 min at 1-min intervals
    int count = 0;
    for (int offset = -45; offset <= 90; offset++) {
        if (count >= MAX_TRACK_POINTS) break;

        unsigned long t = (unsigned long)(now + (offset * 60));
        satellites[satIndex].findsat(t);

        track.lats[count] = satellites[satIndex].satLat;
        track.lons[count] = satellites[satIndex].satLon;
        count++;
    }

    track.count = count;
    return true;
}

int orbit_computeTrail(int satIndex,
                       OrbitTrailPoint* pts, int maxPts,
                       int durationSec, int stepSec) {
    if (satIndex < 0 || satIndex > 1 || !sat_valid[satIndex] || maxPts <= 0) return 0;

    time_t now;
    time(&now);

    int count = 0;
    for (int s = 0; s <= durationSec && count < maxPts; s += stepSec) {
        unsigned long t = (unsigned long)(now - s); // BACKWARD in time
        satellites[satIndex].findsat(t);
        pts[count].lat = satellites[satIndex].satLat;
        pts[count].lng = satellites[satIndex].satLon;
        pts[count].alt = satellites[satIndex].satAlt;
        count++;
    }
    return count;
}

void orbit_setTime() {
    Serial.println("[ORBIT] Time reference set from NTP");
}
