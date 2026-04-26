// HTTP config server for SpaceTracker.
//
//   GET  /            → settings UI (HTML from PROGMEM)
//   GET  /api/state   → JSON of full State struct
//   POST /api/state   → update fields from JSON body, persist, apply
//   GET  /api/status  → uptime / RSSI / NTP / last-fetch ages
//   POST /api/restart → reboot
//   POST /api/reset   → wipe NVS + reboot
//
// Built on the standard ESP32 Arduino WebServer (synchronous, single-threaded).
// More than enough for a config-only UI accessed by one user at a time.

#include "webportal.h"

#include "state.h"
#include "config.h"
#include "display.h"
#include "portal_html.h"  // PORTAL_HTML PROGMEM string

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

extern bool          ntpSynced;
extern unsigned long lastAstrosFetch;
extern unsigned long lastTLEFetch;
extern void          applyBrightness();
extern void          applyLed();

static WebServer server(80);

// ── Helpers ───────────────────────────────────────────────────────────────
static void serializeState(JsonDocument &doc) {
    doc["currentView"]     = state.currentView;
    doc["brightnessLevel"] = state.brightnessLevel;
    doc["currentCity"]     = state.currentCity;
    doc["ledOn"]           = state.ledOn;
    doc["legendsOn"]       = state.legendsOn;
    doc["starsOn"]         = state.starsOn;

    JsonArray ve = doc["viewEnabled"].to<JsonArray>();
    for (int i = 0; i < VIEW_COUNT; i++) ve.add(state.viewEnabled[i]);

    doc["cityCount"] = state.cityCount;
    JsonArray cs = doc["cities"].to<JsonArray>();
    for (int i = 0; i < state.cityCount; i++) {
        JsonObject c = cs.add<JsonObject>();
        c["name"]      = state.cities[i].name;
        c["lat"]       = state.cities[i].lat;
        c["lng"]       = state.cities[i].lng;
        c["centerLat"] = state.cities[i].centerLat;
        c["centerLng"] = state.cities[i].centerLng;
    }

    doc["wifiSsid"]       = state.wifiSsid;
    // never expose password back
    doc["ntpOffsetHours"] = state.ntpOffsetHours;
    doc["ntpDstHours"]    = state.ntpDstHours;
}

static bool deserializeState(const JsonDocument &doc, bool &needsReboot) {
    needsReboot = false;

    // WiFi changes always require reboot
    if (doc["wifiSsid"].is<const char*>()) {
        const char* s = doc["wifiSsid"];
        if (s && strcmp(s, state.wifiSsid) != 0) {
            strncpy(state.wifiSsid, s, sizeof(state.wifiSsid) - 1);
            state.wifiSsid[sizeof(state.wifiSsid) - 1] = 0;
            needsReboot = true;
        }
    }
    if (doc["wifiPass"].is<const char*>()) {
        const char* p = doc["wifiPass"];
        if (p && *p) {
            strncpy(state.wifiPass, p, sizeof(state.wifiPass) - 1);
            state.wifiPass[sizeof(state.wifiPass) - 1] = 0;
            needsReboot = true;
        }
    }

    if (doc["ntpOffsetHours"].is<int>()) state.ntpOffsetHours = (int8_t)doc["ntpOffsetHours"].as<int>();
    if (doc["ntpDstHours"].is<int>())    state.ntpDstHours    = (int8_t)doc["ntpDstHours"].as<int>();

    if (doc["brightnessLevel"].is<int>()) state.brightnessLevel = (uint8_t)doc["brightnessLevel"].as<int>();
    if (doc["ledOn"].is<bool>())          state.ledOn           = doc["ledOn"];
    if (doc["legendsOn"].is<bool>())      state.legendsOn       = doc["legendsOn"];
    if (doc["starsOn"].is<bool>())        state.starsOn         = doc["starsOn"];

    if (doc["viewEnabled"].is<JsonArrayConst>()) {
        JsonArrayConst arr = doc["viewEnabled"];
        int i = 0;
        for (JsonVariantConst v : arr) {
            if (i >= VIEW_COUNT) break;
            state.viewEnabled[i++] = v.as<bool>();
        }
    }

    if (doc["cities"].is<JsonArrayConst>()) {
        JsonArrayConst arr = doc["cities"];
        int i = 0;
        for (JsonVariantConst c : arr) {
            if (i >= MAX_CITIES) break;
            const char* name = c["name"];
            if (name) {
                strncpy(state.cities[i].name, name, sizeof(state.cities[i].name) - 1);
                state.cities[i].name[sizeof(state.cities[i].name) - 1] = 0;
            }
            state.cities[i].lat       = c["lat"]       | state.cities[i].lat;
            state.cities[i].lng       = c["lng"]       | state.cities[i].lng;
            state.cities[i].centerLat = c["centerLat"] | state.cities[i].centerLat;
            state.cities[i].centerLng = c["centerLng"] | state.cities[i].centerLng;
            i++;
        }
        state.cityCount = i;
        if (state.cityCount == 0) state.cityCount = 1;
        if (state.currentCity >= state.cityCount) state.currentCity = 0;
    }
    return true;
}

// ── Route handlers ────────────────────────────────────────────────────────
static void handleRoot() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", PORTAL_HTML);
}

static void handleGetState() {
    JsonDocument doc;
    serializeState(doc);
    String out;
    serializeJson(doc, out);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", out);
}

static void handlePostState() {
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        server.send(400, "application/json", String("{\"error\":\"") + err.c_str() + "\"}");
        return;
    }
    bool needsReboot = false;
    deserializeState(doc, needsReboot);
    state_save();

    // Apply live changes
    applyBrightness();
    applyLed();

    String resp = String("{\"ok\":true,\"reboot\":") + (needsReboot ? "true" : "false") + "}";
    server.send(200, "application/json", resp);

    if (needsReboot) {
        delay(800);
        ESP.restart();
    }
}

static void handleStatus() {
    JsonDocument doc;
    doc["fw"]       = FW_VERSION;
    doc["up"]       = (uint32_t)(millis() / 1000);
    doc["rssi"]     = WiFi.RSSI();
    doc["ip"]       = WiFi.localIP().toString();
    doc["ntp"]      = ntpSynced;
    doc["crewAge"]  = (uint32_t)((millis() - lastAstrosFetch) / 1000);
    doc["tleAge"]   = (uint32_t)((millis() - lastTLEFetch) / 1000);
    String out;
    serializeJson(doc, out);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", out);
}

static void handleRestart() {
    server.send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
}

static void handleReset() {
    state_reset();
    server.send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
}

void webportal_begin() {
    if (MDNS.begin("spacetracker")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("mDNS: spacetracker.local");
    }

    server.on("/",            HTTP_GET,  handleRoot);
    server.on("/api/state",   HTTP_GET,  handleGetState);
    server.on("/api/state",   HTTP_POST, handlePostState);
    server.on("/api/status",  HTTP_GET,  handleStatus);
    server.on("/api/restart", HTTP_POST, handleRestart);
    server.on("/api/reset",   HTTP_POST, handleReset);
    server.onNotFound([](){ server.send(404, "text/plain", "Not found"); });
    server.begin();
    Serial.printf("Webportal: http://%s/\n", WiFi.localIP().toString().c_str());
}

void webportal_loop() {
    server.handleClient();
}
