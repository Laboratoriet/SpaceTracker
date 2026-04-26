// Hardcoded TLE fallback — used when runtime fetch fails (TLS handshake
// errors, rate-limiting from CelesTrak, network outage, etc.).
//
// Keep these reasonably fresh. Update by re-running:
//   curl -s "https://tle.ivanstanojevic.me/api/tle/25544"
//   curl -s "https://tle.ivanstanojevic.me/api/tle/48274"
// and pasting the line1/line2 strings here.
//
// TLE accuracy degrades ~1km/day for ISS as the orbit decays from atmospheric
// drag. After a few weeks the position will visibly drift on the globe view.
//
// Captured: 2026-04-25 14:51 UTC

#pragma once

// ISS (ZARYA) — NORAD 25544
#define TLE_ISS_FALLBACK_NAME  "ISS (ZARYA)"
#define TLE_ISS_FALLBACK_LINE1 "1 25544U 98067A   26115.61933538  .00010271  00000+0  19457-3 0  9992"
#define TLE_ISS_FALLBACK_LINE2 "2 25544  51.6320 200.2872 0006949 349.7097  10.3748 15.48952974563604"

// CSS (TIANHE / Tiangong) — NORAD 48274
#define TLE_CSS_FALLBACK_NAME  "CSS (TIANHE)"
#define TLE_CSS_FALLBACK_LINE1 "1 48274U 21035A   26115.90640323  .00030509  00000+0  32916-3 0  9998"
#define TLE_CSS_FALLBACK_LINE2 "2 48274  41.4670 280.3691 0006843 274.1107  85.8950 15.62962461284990"
