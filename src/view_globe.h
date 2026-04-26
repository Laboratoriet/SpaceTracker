#pragma once

#include "orbit.h"
#include "state.h"

// Renders the globe view (orbit sketch with country map + city marker +
// comet-trail satellite orbits). City selected via state.currentCity.
void view_globe_render(const SatState &iss, const SatState &css);
