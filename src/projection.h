#pragma once

// Orthographic projection helpers for the globe view. Mirrors the JS
// preview's `project()` / `projectAlt()` / `occludedByEarth()` from
// preview/index.html.
//
// All angles in DEGREES (lat, lng, centerLat, centerLng).
// All output coords are screen-space pixel offsets relative to the Earth
// disc center; add (earthCx, earthCy) to draw.

#include <stdint.h>

struct ProjPoint {
    float x;        // pixel offset from disc center (canvas X)
    float y;        // pixel offset from disc center (canvas Y, +y down)
    float z;        // depth — positive = toward camera (visible front side)
    bool  visible;  // z > 0
};

// Project a point on the unit sphere onto the orthographic view centered on
// (centerLat, centerLng). `R` is the visual radius of the sphere in pixels.
ProjPoint projection_project(float lat, float lng,
                             float centerLat, float centerLng,
                             float R);

// Project a point at a given altitude above the surface. `earthR` is the
// visual radius of the Earth disc; `altScalePxPerKm` scales the satellite's
// real altitude (km) into pixels above the disc edge.
ProjPoint projection_projectAlt(float lat, float lng, float altKm,
                                float centerLat, float centerLng,
                                float earthR, float altScalePxPerKm);

// True if a 3D point is hidden behind the Earth sphere (depth-test against
// the visible front face at the same canvas (x,y)).
bool projection_occluded(const ProjPoint& p, float earthR);
