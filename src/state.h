#pragma once

#include <stdint.h>

// Six selectable views, cycled with B1 short-press
enum View : uint8_t {
    VIEW_CREW = 0,
    VIEW_NAMES,
    VIEW_ORBIT,     // device-faithful 2D ellipse view
    VIEW_GLOBE,     // new — country map + city + comet-trail orbits
    VIEW_CLOCK,     // new — time + optional starfield
    VIEW_DAYLIGHT,  // new — daylight tracker
    VIEW_COUNT
};

enum BrightnessLevel : uint8_t {
    BRIGHTNESS_LEVEL_LOW = 0,
    BRIGHTNESS_LEVEL_MID,
    BRIGHTNESS_LEVEL_HIGH,
};

#define MAX_CITIES 5

struct CityEntry {
    char  name[16];        // null-terminated, fits "San Francisco"
    float lat;             // degrees
    float lng;             // degrees
    float centerLat;       // projection center for the globe view
    float centerLng;       // projection center for the globe view
};

// Device-wide persistent state. Loaded from NVS on boot, saved on every change.
struct State {
    // Display / view state
    uint8_t currentView;       // View enum value
    uint8_t brightnessLevel;   // BrightnessLevel enum value
    uint8_t currentCity;       // index into cities[]
    bool    ledOn;             // GPIO 38 (green status LED)
    bool    legendsOn;         // status bars + daylight message visibility
    bool    starsOn;           // starfield on clock view

    // Per-view enable flags — disabled views are skipped in the cycle
    bool    viewEnabled[VIEW_COUNT];

    // City list (user-editable via web portal)
    uint8_t   cityCount;       // number of valid cities (1..MAX_CITIES)
    CityEntry cities[MAX_CITIES];

    // WiFi credentials
    char    wifiSsid[33];      // 32 chars + null
    char    wifiPass[65];      // 64 chars + null

    // Time settings (NTP)
    int8_t  ntpOffsetHours;    // -12..+14
    int8_t  ntpDstHours;       // 0 or 1
};

extern State state;

// NVS persistence
void state_load();   // populate `state` from NVS, fall back to defaults
void state_save();   // persist current `state` to NVS
void state_reset();  // clear NVS, restore in-memory defaults

// Convenience helpers for cycle-style mutations (each calls state_save)
void state_cycleView();        // skips disabled views
void state_cycleBrightness();
void state_cycleCity();
void state_toggleLed();
void state_toggleLegends();
void state_toggleStars();

// True if the user has at least one valid view enabled
bool state_anyViewEnabled();
