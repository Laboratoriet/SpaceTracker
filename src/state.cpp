#include "state.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

State state;  // populated by state_load() (calls state_init_defaults_ first)

// Apply default values to the in-memory state. Called from state_load() so
// it always runs explicitly — never relying on C++ static init order.
static void state_init_defaults() {
    state.currentView     = VIEW_CREW;
    state.brightnessLevel = BRIGHTNESS_LEVEL_HIGH;
    state.currentCity     = 0;
    state.ledOn           = true;
    state.legendsOn       = true;
    state.starsOn         = true;
    for (int i = 0; i < VIEW_COUNT; i++) state.viewEnabled[i] = true;
    state.cityCount = 2;
    strncpy(state.cities[0].name, "Oslo", sizeof(state.cities[0].name));
    state.cities[0].lat       = 59.9139f;
    state.cities[0].lng       = 10.7522f;
    state.cities[0].centerLat = 30.0f;
    state.cities[0].centerLng = 10.0f;
    strncpy(state.cities[1].name, "Warsaw", sizeof(state.cities[1].name));
    state.cities[1].lat       = 52.2297f;
    state.cities[1].lng       = 21.0122f;
    state.cities[1].centerLat = 30.0f;
    state.cities[1].centerLng = 21.0f;
    state.wifiSsid[0]    = 0;
    state.wifiPass[0]    = 0;
    state.ntpOffsetHours = 1;   // CET base
    state.ntpDstHours    = 1;   // +1h DST
}

static Preferences prefs;
static const char* NAMESPACE = "tracker";

// Bump on layout / default changes to invalidate stale NVS from older builds
static constexpr uint8_t STATE_VERSION = 3;

void state_load() {
    state_init_defaults();           // always start from defaults
    prefs.begin(NAMESPACE, true);    // read-only
    uint8_t storedVersion = prefs.getUChar("ver", 0);
    if (storedVersion != STATE_VERSION) {
        // Fresh / outdated NVS — keep in-memory defaults, write the new
        // version on next save. Don't read other keys (they may be wrong).
        prefs.end();
        state_save();
        return;
    }

    state.currentView     = prefs.getUChar("view",   state.currentView);
    state.brightnessLevel = prefs.getUChar("bright", state.brightnessLevel);
    state.currentCity     = prefs.getUChar("city",   state.currentCity);
    state.ledOn           = prefs.getBool ("led",    state.ledOn);
    state.legendsOn       = prefs.getBool ("legend", state.legendsOn);
    state.starsOn         = prefs.getBool ("stars",  state.starsOn);

    prefs.getBytes("vEna", state.viewEnabled, sizeof(state.viewEnabled));
    state.cityCount = prefs.getUChar("nCity", state.cityCount);
    if (state.cityCount > MAX_CITIES) state.cityCount = MAX_CITIES;
    if (state.cityCount > 0) {
        prefs.getBytes("cities", state.cities,
                       sizeof(CityEntry) * state.cityCount);
    }

    prefs.getString("wifiSsid", state.wifiSsid, sizeof(state.wifiSsid));
    prefs.getString("wifiPass", state.wifiPass, sizeof(state.wifiPass));
    state.ntpOffsetHours = (int8_t)prefs.getChar("ntpOff", state.ntpOffsetHours);
    state.ntpDstHours    = (int8_t)prefs.getChar("ntpDst", state.ntpDstHours);
    prefs.end();

    // Defensive clamps
    if (state.currentView     >= VIEW_COUNT)            state.currentView     = VIEW_CREW;
    if (state.brightnessLevel >  BRIGHTNESS_LEVEL_HIGH) state.brightnessLevel = BRIGHTNESS_LEVEL_MID;
    if (state.currentCity     >= state.cityCount)       state.currentCity     = 0;
    if (state.cityCount       == 0)                     state.cityCount       = 1; // need at least one
}

void state_save() {
    prefs.begin(NAMESPACE, false); // read-write
    prefs.putUChar("ver",    STATE_VERSION);
    prefs.putUChar("view",   state.currentView);
    prefs.putUChar("bright", state.brightnessLevel);
    prefs.putUChar("city",   state.currentCity);
    prefs.putBool ("led",    state.ledOn);
    prefs.putBool ("legend", state.legendsOn);
    prefs.putBool ("stars",  state.starsOn);

    prefs.putBytes("vEna", state.viewEnabled, sizeof(state.viewEnabled));
    prefs.putUChar("nCity", state.cityCount);
    prefs.putBytes("cities", state.cities,
                   sizeof(CityEntry) * state.cityCount);

    prefs.putString("wifiSsid", state.wifiSsid);
    prefs.putString("wifiPass", state.wifiPass);
    prefs.putChar  ("ntpOff",   (char)state.ntpOffsetHours);
    prefs.putChar  ("ntpDst",   (char)state.ntpDstHours);
    prefs.end();
}

void state_reset() {
    prefs.begin(NAMESPACE, false);
    prefs.clear();
    prefs.end();
}

void state_cycleView() {
    if (!state_anyViewEnabled()) return;
    // Skip disabled views — loop max VIEW_COUNT times to avoid infinite hang
    for (int i = 0; i < VIEW_COUNT; i++) {
        state.currentView = (state.currentView + 1) % VIEW_COUNT;
        if (state.viewEnabled[state.currentView]) break;
    }
    state_save();
}

void state_cycleBrightness() {
    state.brightnessLevel = (state.brightnessLevel + 1) % 3;
    state_save();
}

void state_cycleCity() {
    if (state.cityCount == 0) return;
    state.currentCity = (state.currentCity + 1) % state.cityCount;
    state_save();
}

void state_toggleLed()     { state.ledOn     = !state.ledOn;     state_save(); }
void state_toggleLegends() { state.legendsOn = !state.legendsOn; state_save(); }
void state_toggleStars()   { state.starsOn   = !state.starsOn;   state_save(); }

bool state_anyViewEnabled() {
    for (int i = 0; i < VIEW_COUNT; i++) {
        if (state.viewEnabled[i]) return true;
    }
    return false;
}
