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

    // The Space Devs is HTTPS — fresh TLS client per call (see note above).
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(15);

    HTTPClient http;
    http.setTimeout(15000);
    http.setReuse(false);
    if (!http.begin(client, ASTROS_URL)) {
        Serial.println("[API] Astros http.begin failed");
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[API] Astros HTTP %d\n", httpCode);
        http.end();
        return false;
    }

    // Read the whole response into a String first. The payload is ~25 KB —
    // small relative to the 8 MB PSRAM — and a String is far more reliable
    // than streaming straight into ArduinoJson over HTTPS (chunked transfer
    // + TLS framing has bitten us before).
    String payload = http.getString();
    http.end();
    Serial.printf("[API] Astros payload len=%d\n", payload.length());
    if (payload.length() < 50) {
        Serial.println("[API] Astros payload too short");
        return false;
    }

    // Filter so ArduinoJson only allocates the few fields we actually need.
    // For arrays, the documented pattern is `filter[key][0]` — a one-element
    // array template matched against every element of the input array.
    JsonDocument filter;
    filter["results"][0]["name"] = true;
    filter["results"][0]["type"]["name"] = true;
    filter["results"][0]["agency"]["country_code"] = true;
    filter["results"][0]["agency"]["name"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, payload,
        DeserializationOption::Filter(filter)
    );

    if (err) {
        Serial.printf("[API] JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonArray results = doc["results"].as<JsonArray>();
    Serial.printf("[API] TSD returned %d results\n", (int)results.size());

    int kept = 0;
    for (JsonObject p : results) {
        if (kept >= 20) break;
        const char* typeName = p["type"]["name"] | "";
        // Skip placeholder entries (e.g. "Starman" — type.name == "Non-Human").
        if (strcmp(typeName, "Non-Human") == 0) continue;

        const char* country = p["agency"]["country_code"] | "";
        crew.names[kept] = p["name"].as<String>();
        crew.crafts[kept] = (strcmp(country, "CHN") == 0) ? "Tiangong" : "ISS";
        kept++;
    }

    if (kept == 0) {
        Serial.println("[API] Crew: filter produced 0 entries — keeping previous data");
        return false;
    }

    crew.count = kept;
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
