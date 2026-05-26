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
// Captured: 2026-05-26 UTC

#pragma once

// ISS (ZARYA) — NORAD 25544
#define TLE_ISS_FALLBACK_NAME  "ISS (ZARYA)"
#define TLE_ISS_FALLBACK_LINE1 "1 25544U 98067A   26145.42220529  .00009654  00000+0  18076-3 0  9992"
#define TLE_ISS_FALLBACK_LINE2 "2 25544  51.6330  52.7989 0007470  96.3597 263.8242 15.49365963568228"

// CSS (TIANHE / Tiangong) — NORAD 48274
#define TLE_CSS_FALLBACK_NAME  "CSS (TIANHE)"
#define TLE_CSS_FALLBACK_LINE1 "1 48274U 21035A   26145.51067809  .00014512  00000+0  17905-3 0  9997"
#define TLE_CSS_FALLBACK_LINE2 "2 48274  41.4679  99.9009 0011050 279.1269  80.8319 15.59810900289621"
