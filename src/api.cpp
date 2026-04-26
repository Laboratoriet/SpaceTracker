#include "api.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// We deliberately do NOT cache the secure client between calls — on ESP32 a
// stale socket / leftover TLS state from a prior request frequently causes
// the next HTTPS handshake to fail silently. A fresh client per call costs
// ~1 second of TLS handshake but is much more reliable.

bool api_fetchCrew(CrewInfo &crew) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(ASTROS_URL);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[API] Astros HTTP %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[API] JSON parse error: %s\n", err.c_str());
        return false;
    }

    crew.count = doc["number"] | 0;
    JsonArray people = doc["people"];
    int i = 0;
    for (JsonObject p : people) {
        if (i >= 20) break;
        crew.names[i] = p["name"].as<String>();
        crew.crafts[i] = p["craft"].as<String>();
        i++;
    }
    crew.valid = true;
    crew.lastFetch = millis();

    Serial.printf("[API] Crew: %d people in space\n", crew.count);
    return true;
}

bool api_fetchTLE(const char* url, TLEData &tle) {
    if (WiFi.status() != WL_CONNECTED) return false;

    // Fresh client per request — see note above
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(15);

    HTTPClient http;
    http.setTimeout(15000);
    http.setReuse(false);
    if (!http.begin(client, url)) {
        Serial.println("[API] http.begin failed");
        return false;
    }

    int httpCode = http.GET();
    Serial.printf("[API] TLE GET %s -> %d\n", url, httpCode);
    if (httpCode != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    Serial.printf("[API] TLE payload len=%d\n", payload.length());

    // TLE format: 3 lines
    int nl1 = payload.indexOf('\n');
    int nl2 = payload.indexOf('\n', nl1 + 1);

    if (nl1 < 0 || nl2 < 0) {
        Serial.println("[API] TLE parse failed - not enough lines");
        return false;
    }

    tle.name = payload.substring(0, nl1);
    tle.name.trim();
    tle.line1 = payload.substring(nl1 + 1, nl2);
    tle.line1.trim();
    tle.line2 = payload.substring(nl2 + 1);
    tle.line2.trim();

    if (!tle.line1.startsWith("1 ") || !tle.line2.startsWith("2 ")) {
        Serial.println("[API] TLE validation failed");
        return false;
    }

    tle.valid = true;
    tle.lastFetch = millis();

    Serial.printf("[API] TLE: %s\n", tle.name.c_str());
    return true;
}
