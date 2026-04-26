#include "projection.h"

#include <math.h>

static constexpr float DEG2RAD = (float)(M_PI / 180.0);

ProjPoint projection_project(float lat, float lng,
                             float centerLat, float centerLng,
                             float R) {
    const float phi  = lat * DEG2RAD;
    const float lam  = (lng - centerLng) * DEG2RAD;
    const float phi0 = centerLat * DEG2RAD;

    const float ux = cosf(phi) * sinf(lam);
    const float uy = sinf(phi);
    const float uz = cosf(phi) * cosf(lam);

    // Rotate by +phi0 around the X axis so (centerLat, centerLng) maps to
    // (0, 0, 1) — directly facing the camera.
    const float yp = uy * cosf(phi0) - uz * sinf(phi0);
    const float zp = uy * sinf(phi0) + uz * cosf(phi0);

    ProjPoint p;
    p.x       = ux * R;
    p.y       = -yp * R;   // canvas Y is inverted (south = +y)
    p.z       = zp;        // dimensionless depth (will be scaled if needed)
    p.visible = (zp > 0.0f);
    return p;
}

ProjPoint projection_projectAlt(float lat, float lng, float altKm,
                                float centerLat, float centerLng,
                                float earthR, float altScalePxPerKm) {
    const float phi  = lat * DEG2RAD;
    const float lam  = (lng - centerLng) * DEG2RAD;
    const float phi0 = centerLat * DEG2RAD;

    const float ux = cosf(phi) * sinf(lam);
    const float uy = sinf(phi);
    const float uz = cosf(phi) * cosf(lam);

    const float yp = uy * cosf(phi0) - uz * sinf(phi0);
    const float zp = uy * sinf(phi0) + uz * cosf(phi0);

    const float R = earthR + altKm * altScalePxPerKm;

    ProjPoint p;
    p.x       = ux * R;
    p.y       = -yp * R;
    p.z       = zp * R;          // depth in same pixel units as x/y
    p.visible = (zp > 0.0f);
    return p;
}

bool projection_occluded(const ProjPoint& p, float earthR) {
    const float r2 = p.x * p.x + p.y * p.y;
    const float er2 = earthR * earthR;
    if (r2 >= er2) return false; // outside the disc — never occluded
    const float zFront = sqrtf(er2 - r2);
    return p.z < zFront;
}
