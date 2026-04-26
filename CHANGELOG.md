# Changelog

All notable changes to SpaceTracker will be documented in this file.

## [1.0.1] — 2026-04-26

First-flash fixes after hardware testing.

### Fixed
- `view_splash_render()` now always calls `display_flush()` internally —
  the captive-portal splash transitions ("Connecting…" → "Join SpaceTracker-XXXX")
  weren't appearing on the AMOLED because callers forgot to flush the canvas.
- State defaults are now applied via an explicit `state_init_defaults()` call
  inside `state_load()` instead of a C++ static-constructor pattern — avoids
  the C++ static initialization order fiasco entirely.
- Added an early 4-blink LED sign-of-life at the top of `setup()` so it's
  always obvious whether the board reached user code, even if Serial /
  display fail later.
- Pinned `moononournation/GFX Library for Arduino` to `~1.4.0` — newer 1.6+
  versions require an ESP32 core header (`esp32-hal-periman.h`) the pinned
  framework version doesn't ship.

### Documentation
- New troubleshooting entry: "Display stays blank but the green LED blinks
  at boot" (the `display_flush()` foot-gun).

## [1.0.0] — 2026-04-26

First public release.

### Six selectable views
- Crew count — big number of humans currently in space
- Crew names — grouped by spacecraft (ISS, Tiangong, etc.)
- Orbit (simple) — Earth-centered tilted ellipses with satellite dots
- Orbit globe — country-outline map projected on the Earth disc with a
  city marker and comet-trail orbits
- Clock screensaver — large 24h time + date with optional starfield
- Daylight tracker — sun arc + sunrise/sunset + narrative message,
  inspired by [bakkenbaeck/daylight-ios](https://github.com/bakkenbaeck/daylight-ios)

### Hardware
- LilyGo T-Display S3 AMOLED (RM67162, 240×536, ESP32-S3, 8 MB PSRAM)
- Tested on the non-touch variant where GPIO 38 = green status LED
- Canvas double-buffering in PSRAM eliminates redraw flicker

### Configuration
- First-boot WiFi captive portal — no hardcoded credentials
- Web config portal at `http://spacetracker.local/` — change cities,
  active views, brightness, LED, font/theme without re-flashing
- All settings persisted in NVS

### Data
- ISS / Tiangong orbits propagated locally via SGP4 (`hopperpop/Sgp4`)
- TLEs fetched from CelesTrak with hardcoded fallback (works offline)
- Crew data from Open Notify (`api.open-notify.org/astros.json`)
- Sunrise/sunset/altitude via hand-ported subset of suncalc.js
- Country outlines from Natural Earth 110m (43 KB PROGMEM)

### Controls
- B1 short = cycle view, B1 long = toggle info text / starfield,
  B1 double-click = swap city on globe view
- B2 short = cycle brightness Low/Med/High, B2 long = toggle LED

### Build
- `pio run` builds clean. Flash via `python3 -m esptool` (see README).
