#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include <WiFi.h>
#include <OneButton.h>
#include <time.h>

#include "pin_config.h"
#include "config.h"
#include "display.h"
#include "api.h"
#include "orbit.h"
#include "state.h"
#include "view_crew.h"
#include "view_names.h"
#include "view_orbit.h"
#include "view_globe.h"
#include "view_clock.h"
#include "view_daylight.h"
#include "view_splash.h"
#include "tle_fallback.h"
#include "wifi_setup.h"
#include "webportal.h"
#include <ESPmDNS.h>

// ── Buttons ──
OneButton btn1(PIN_BUTTON_1, true);
OneButton btn2(PIN_BUTTON_2, true);

// ── Data ──
CrewInfo crew = {};
TLEData tleISS = {};
TLEData tleTiangong = {};
SatState issState = {};
SatState cssState = {};

bool wifiConnected = false;
bool ntpSynced = false;
bool dirty = true;

// ── Timing ──
unsigned long lastAstrosFetch = 0;
unsigned long lastTLEFetch = 0;
unsigned long lastOrbitUpdate = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastClockTick = 0;
unsigned long lastDaylightTick = 0;
unsigned long lastGlobeTick = 0;

// ── Startup state machine ──
enum StartupPhase { BOOT_DONE, FETCH_CREW, FETCH_ISS_TLE, FETCH_CSS_TLE, STARTUP_COMPLETE };
StartupPhase startupPhase = BOOT_DONE;

// ── Forward declarations ──
void syncNTP();
void updateOrbits();
void applyBrightness();
void applyLed();
void renderCurrent();
void loadFallbackTLEs();

// ── Helpers — apply state to hardware ─────────────────────────────────────
void applyBrightness() {
    uint8_t v = BRIGHTNESS_MID;
    switch (state.brightnessLevel) {
        case BRIGHTNESS_LEVEL_LOW:  v = BRIGHTNESS_LOW;  break;
        case BRIGHTNESS_LEVEL_MID:  v = BRIGHTNESS_MID;  break;
        case BRIGHTNESS_LEVEL_HIGH: v = BRIGHTNESS_HIGH; break;
    }
    display_setBrightness(v);
}

void applyLed() {
    digitalWrite(PIN_LED, state.ledOn ? HIGH : LOW);
}

// ── Button callbacks ─────────────────────────────────────────────────────
void onBtn1Click() {
    state_cycleView();
    dirty = true;
    Serial.printf("View: %d\n", state.currentView);
}

void onBtn1LongPress() {
    // Context-sensitive: on the clock view this toggles the starfield
    // (so the user can switch between screensaver vs minimal time);
    // on every other view it toggles the legends/info-text overlay.
    if (state.currentView == VIEW_CLOCK) {
        state_toggleStars();
        Serial.printf("Stars: %s\n", state.starsOn ? "ON" : "OFF");
    } else {
        state_toggleLegends();
        Serial.printf("Legends: %s\n", state.legendsOn ? "ON" : "OFF");
    }
    dirty = true;
}

void onBtn1DoubleClick() {
    // Context: city swap only makes sense on the globe view
    if (state.currentView == VIEW_GLOBE) {
        state_cycleCity();
        Serial.printf("City: %d\n", state.currentCity);
        dirty = true;
    }
}

void onBtn2Click() {
    state_cycleBrightness();
    applyBrightness();
    Serial.printf("Brightness: %d\n", state.brightnessLevel);
}

void onBtn2LongPress() {
    state_toggleLed();
    applyLed();
    Serial.printf("LED: %s\n", state.ledOn ? "ON" : "OFF");
}

// ── NTP / Orbit ─────────────────────────────────────────────────────────
// connectWiFi() lives in src/wifi_setup.cpp now.
void syncNTP() {
    long gmtOff = (long)state.ntpOffsetHours * 3600L;
    long dstOff = (long)state.ntpDstHours    * 3600L;
    configTime(gmtOff, dstOff, NTP_SERVER);
    struct tm tinfo;
    int attempts = 0;
    while (!getLocalTime(&tinfo) && attempts < 10) { delay(500); attempts++; }
    if (getLocalTime(&tinfo)) {
        ntpSynced = true;
        orbit_setTime();
        Serial.println("NTP synced");
    } else {
        Serial.println("NTP sync failed");
    }
}

void updateOrbits() {
    if (!ntpSynced) return;
    if (issState.tleLoaded) {
        orbit_getPosition(SAT_ISS, issState.current);
        orbit_computeTrack(SAT_ISS, issState.track);
    }
    if (cssState.tleLoaded) {
        orbit_getPosition(SAT_TIANGONG, cssState.current);
        orbit_computeTrack(SAT_TIANGONG, cssState.track);
    }
    lastOrbitUpdate = millis();
}

// Load the compiled-in TLEs as a fallback so the orbit views always have
// something to show. Used when remote fetch fails (TLS errors, rate-limit,
// no WiFi, etc.). The data degrades by a few km/day so re-flashing every
// month or two is recommended for accuracy.
void loadFallbackTLEs() {
    Serial.println("Loading compiled-in TLE fallback...");
    tleISS.name  = TLE_ISS_FALLBACK_NAME;
    tleISS.line1 = TLE_ISS_FALLBACK_LINE1;
    tleISS.line2 = TLE_ISS_FALLBACK_LINE2;
    tleISS.valid = true;
    if (orbit_initSat(SAT_ISS, tleISS)) {
        issState.tleLoaded = true;
        Serial.println("  ISS fallback OK");
    }

    tleTiangong.name  = TLE_CSS_FALLBACK_NAME;
    tleTiangong.line1 = TLE_CSS_FALLBACK_LINE1;
    tleTiangong.line2 = TLE_CSS_FALLBACK_LINE2;
    tleTiangong.valid = true;
    if (orbit_initSat(SAT_TIANGONG, tleTiangong)) {
        cssState.tleLoaded = true;
        Serial.println("  CSS fallback OK");
    }
}

// ── Render dispatch ──────────────────────────────────────────────────────
void renderCurrent() {
    switch (state.currentView) {
        case VIEW_CREW:     view_crew_render(crew, wifiConnected);          break;
        case VIEW_NAMES:    view_names_render(crew);                        break;
        case VIEW_ORBIT:    view_orbit_render(issState, cssState, wifiConnected); break;
        case VIEW_GLOBE:    view_globe_render(issState, cssState);          break;
        case VIEW_CLOCK:    view_clock_render();                            break;
        case VIEW_DAYLIGHT: view_daylight_render();                         break;
    }
}

// ── Setup ────────────────────────────────────────────────────────────────
void setup() {
    // Earliest sign-of-life: blink the green LED so we can see the board
    // reached user code even if Serial / display fail later.
    pinMode(PIN_LED, OUTPUT);
    for (int i = 0; i < 4; i++) {
        digitalWrite(PIN_LED, HIGH); delay(120);
        digitalWrite(PIN_LED, LOW);  delay(120);
    }
    digitalWrite(PIN_LED, HIGH);  // leave on

    Serial.begin(115200);
    delay(2500);  // give USB-CDC time to enumerate so first prints are caught
    Serial.println("\n=== Space Tracker boot ===");
    Serial.flush();

    Serial.println("[BOOT] before state_load");
    Serial.flush();
    state_load();
    Serial.println("[BOOT] after state_load");
    Serial.flush();

    // (diagnostic short-circuit removed — proper flow continues below)
    Serial.printf("State: view=%d bright=%d city=%d led=%d legends=%d stars=%d\n",
                  state.currentView, state.brightnessLevel, state.currentCity,
                  state.ledOn, state.legendsOn, state.starsOn);

    Serial.println("[BOOT] before display_init");
    display_init();
    Serial.println("[BOOT] after display_init");
    applyBrightness();
    Serial.println("[BOOT] after applyBrightness");
    applyLed();
    Serial.println("[BOOT] after applyLed");
    view_clock_initStars();
    Serial.println("[BOOT] after initStars");

    btn1.attachClick(onBtn1Click);
    btn1.attachLongPressStart(onBtn1LongPress);
    btn1.attachDoubleClick(onBtn1DoubleClick);
    btn1.setPressMs(LONG_PRESS_MS);
    btn2.attachClick(onBtn2Click);
    btn2.attachLongPressStart(onBtn2LongPress);
    btn2.setPressMs(LONG_PRESS_MS);

    // Load fallback TLEs immediately — orbit views work even if WiFi/TLS
    // never succeeds. A successful runtime fetch will overwrite these.
    loadFallbackTLEs();
    Serial.println("[BOOT] after loadFallbackTLEs");

    // First-boot WiFi flow: try saved creds → on failure or empty creds,
    // launch captive portal AP. While in portal mode `wifi_setup_loop()`
    // takes over and we never proceed past WiFi setup.
    view_splash_render("Connecting...");
    Serial.println("[BOOT] after splash render");
    bool wifiOK = wifi_setup_connect_or_portal();
    Serial.printf("[BOOT] wifi_setup returned, wifiOK=%d, portal=%d\n",
                  wifiOK, wifi_setup_in_portal_mode());
    wifiConnected = wifiOK;

    if (wifi_setup_in_portal_mode()) {
        // Stay in portal mode forever (until reboot after creds saved)
        startupPhase = BOOT_DONE;  // doesn't matter — loop will only service portal
        return;
    }

    if (wifiConnected) {
        view_splash_render("Syncing time...");
        syncNTP();
        // Show the local IP for a beat so the user knows where to find the portal
        char line[64];
        snprintf(line, sizeof(line), "%s · spacetracker.local",
                 WiFi.localIP().toString().c_str());
        view_splash_render(line);
        delay(2500);
        view_splash_render("Loading data...");
        webportal_begin();
    }

    startupPhase = FETCH_CREW;
    // Intentionally not setting dirty=true here — let the splash linger
    // until the startup state machine sets dirty after STARTUP_COMPLETE.
}

// ── Loop ─────────────────────────────────────────────────────────────────
void loop() {
    // Captive-portal mode: only service portal traffic, skip everything else
    if (wifi_setup_in_portal_mode()) {
        wifi_setup_loop();
        delay(2);
        return;
    }

    btn1.tick();
    btn2.tick();
    webportal_loop();

    unsigned long now = millis();

    // Startup data fetch (one step per loop). Splash updates with progress
    // text so the boot doesn't feel frozen while HTTPS fetches run.
    if (startupPhase != STARTUP_COMPLETE && wifiConnected) {
        switch (startupPhase) {
            case FETCH_CREW:
                Serial.println("Fetching crew...");
                view_splash_render("Loading crew...");
                api_fetchCrew(crew);
                lastAstrosFetch = now;
                startupPhase = FETCH_ISS_TLE;
                break;
            case FETCH_ISS_TLE:
                Serial.println("Fetching ISS TLE...");
                view_splash_render("Loading ISS orbit...");
                if (api_fetchTLE(TLE_ISS_URL, tleISS)) {
                    orbit_initSat(SAT_ISS, tleISS);
                    issState.tleLoaded = true;
                }
                startupPhase = FETCH_CSS_TLE;
                break;
            case FETCH_CSS_TLE:
                Serial.println("Fetching Tiangong TLE...");
                view_splash_render("Loading CSS orbit...");
                if (api_fetchTLE(TLE_TIANGONG_URL, tleTiangong)) {
                    orbit_initSat(SAT_TIANGONG, tleTiangong);
                    cssState.tleLoaded = true;
                }
                lastTLEFetch = now;
                updateOrbits();
                startupPhase = STARTUP_COMPLETE;
                dirty = true;  // first real-view render happens here
                Serial.println("Startup complete");
                break;
            default: break;
        }
    }

    // WiFi reconnect — uses creds from state (set via captive portal)
    bool wasConnected = wifiConnected;
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (!wifiConnected && state.wifiSsid[0] != 0 &&
        (now - lastWiFiCheck > WIFI_RETRY_MS)) {
        lastWiFiCheck = now;
        Serial.println("Reconnecting WiFi...");
        WiFi.disconnect();
        WiFi.begin(state.wifiSsid, state.wifiPass);
    }
    if (wifiConnected && !wasConnected) {
        Serial.println("WiFi reconnected");
        if (!ntpSynced) syncNTP();
        dirty = true;
    }

    // Periodic crew refresh — aggressive retry until first success
    unsigned long crewInterval = crew.valid ? ASTROS_REFRESH_MS : 30UL * 1000;
    if (wifiConnected && startupPhase == STARTUP_COMPLETE &&
        (now - lastAstrosFetch > crewInterval)) {
        Serial.println("Refreshing crew...");
        bool ok = api_fetchCrew(crew);
        Serial.printf("  result: %s (count=%d valid=%d)\n",
                      ok ? "ok" : "fail", crew.count, crew.valid ? 1 : 0);
        lastAstrosFetch = now;
        if (state.currentView == VIEW_CREW || state.currentView == VIEW_NAMES) dirty = true;
    }

    // Periodic TLE refresh — try every 6 hours regardless of fallback state,
    // and BE KIND to CelesTrak (they rate-limit aggressive scrapers and have
    // already blocked this IP once for 2 hours). The fallback TLEs already
    // give us valid positions so there's no benefit to fast retries.
    if (wifiConnected && startupPhase == STARTUP_COMPLETE &&
        (now - lastTLEFetch > TLE_REFRESH_MS)) {
        Serial.println("Refreshing TLEs from network...");
        if (api_fetchTLE(TLE_ISS_URL, tleISS)) {
            orbit_initSat(SAT_ISS, tleISS);
            issState.tleLoaded = true;
            Serial.println("  ISS TLE: ok (network)");
        }
        if (api_fetchTLE(TLE_TIANGONG_URL, tleTiangong)) {
            orbit_initSat(SAT_TIANGONG, tleTiangong);
            cssState.tleLoaded = true;
            Serial.println("  CSS TLE: ok (network)");
        }
        lastTLEFetch = now;
        updateOrbits();
        if (state.currentView == VIEW_ORBIT || state.currentView == VIEW_GLOBE) dirty = true;
    }

    // SGP4 position recalc every 10 seconds
    if (ntpSynced && (now - lastOrbitUpdate > ORBIT_UPDATE_MS)) {
        updateOrbits();
        if (state.currentView == VIEW_ORBIT || state.currentView == VIEW_GLOBE) dirty = true;
    }

    // Per-view tick cadences — bumped longer so the device feels stable.
    // Canvas double-buffering already eliminates the redraw flash; lower
    // frequency just makes the device feel more "still".
    if (state.currentView == VIEW_ORBIT || state.currentView == VIEW_GLOBE) {
        // Satellites move ~4°/min on the orbit; 2 min is plenty visible.
        if (now - lastGlobeTick > 120000UL) { lastGlobeTick = now; dirty = true; }
    }
    if (state.currentView == VIEW_CLOCK) {
        // 30s tick — needs to catch the minute roll-over within ~30s
        if (now - lastClockTick > 30000UL) { lastClockTick = now; dirty = true; }
    }
    if (state.currentView == VIEW_DAYLIGHT) {
        // 60s tick — slow enough that the AMOLED feels still, fast enough
        // that the gradient drift through the day is actually visible.
        if (now - lastDaylightTick > 60000UL) { lastDaylightTick = now; dirty = true; }
    }

    if (dirty) {
        dirty = false;
        renderCurrent();
        display_flush();   // push canvas → AMOLED in one go (no flicker)
    }

    delay(10);
}
