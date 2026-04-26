#pragma once

// Run once at boot to populate the deterministic starfield.
void view_clock_initStars();

// Renders the clock screensaver — big 24h time + date, optional starfield.
// Starfield visibility comes from state.starsOn.
void view_clock_render();
