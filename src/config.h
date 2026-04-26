#pragma once

// ── Firmware version (shown in webportal + status) ──
#define FW_VERSION "1.0.0"

// WiFi credentials no longer hardcoded — set via captive-portal at first boot.
// See src/wifi_setup.cpp.

// ── Display ──
#define DISP_W 536
#define DISP_H 240

// ── API endpoints ──
#define ASTROS_URL       "http://api.open-notify.org/astros.json"
#define TLE_ISS_URL      "https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=TLE"
#define TLE_TIANGONG_URL "https://celestrak.org/NORAD/elements/gp.php?CATNR=48274&FORMAT=TLE"

// ── NTP ──
// Oslo / Warsaw both use Central European Time (CET = UTC+1, CEST = UTC+2 in
// summer). Set base offset to UTC+1 hour with a 1-hour DST shift; the ESP32
// time library applies the DST offset automatically when in DST window.
#define NTP_SERVER       "pool.ntp.org"
#define NTP_GMT_OFFSET   3600       // +1h base
#define NTP_DST_OFFSET   3600       // +1h DST

// ── Timing (ms) ──
#define ASTROS_REFRESH_MS    (5UL * 60 * 1000)     // 5 minutes
#define TLE_REFRESH_MS       (6UL * 60 * 60 * 1000) // 6 hours
#define ORBIT_UPDATE_MS      (60UL * 1000)           // 60 seconds — slow enough to avoid visible refresh blink
#define WIFI_RETRY_MS        (30UL * 1000)

// ── Colors (RGB565) ──
// Pure black = AMOLED pixels off
#define COL_BG          0x0000

// Coastlines: dim blue-gray
#define COL_COASTLINE   0x2945  // ~(40,50,45)

// ISS: cyan
#define COL_ISS_DOT     0x07FF  // bright cyan
#define COL_ISS_TRACK   0x0314  // dim cyan

// Tiangong: amber
#define COL_CSS_DOT     0xFBE0  // bright amber
#define COL_CSS_TRACK   0x8B20  // dim amber

// Text
#define COL_TEXT_BRIGHT 0xE73C  // cool white
#define COL_TEXT_DIM    0x7BEF  // medium gray
#define COL_TEXT_DARK   0x4208  // dark gray

// Status / accents
#define COL_WIFI_OK     0x07E0  // green
#define COL_WIFI_ERR    0xF800  // red
#define COL_STALE       0xFD20  // orange

// ── Brightness ──
#define DEFAULT_BRIGHTNESS 180
#define MIN_BRIGHTNESS     10
#define MAX_BRIGHTNESS     255

// Discrete cycle levels (cycle on B2 short-press) — bumped after first
// hardware test showed MID=130 reads as "totally dim" on this AMOLED.
#define BRIGHTNESS_LOW     80
#define BRIGHTNESS_MID     180
#define BRIGHTNESS_HIGH    255

// ── Buttons ──
#define LONG_PRESS_MS      800

// ── Cities of interest (for the globe + daylight views) ──
#define OSLO_LAT      59.9139f
#define OSLO_LNG      10.7522f
#define WARSAW_LAT    52.2297f
#define WARSAW_LNG    21.0122f

// Per-city projection center — picks an orthographic vantage point that puts
// the city about 1/3 from the top of the visible Earth disc.
#define OSLO_CENTER_LAT    30.0f
#define OSLO_CENTER_LNG    10.0f
#define WARSAW_CENTER_LAT  30.0f
#define WARSAW_CENTER_LNG  21.0f
