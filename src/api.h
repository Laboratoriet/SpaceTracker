#pragma once

#include <Arduino.h>

// Astronaut data
struct CrewInfo {
    int count;
    String names[20];   // max 20 astronauts
    String crafts[20];
    bool valid;
    unsigned long lastFetch;
};

// TLE data
struct TLEData {
    String name;
    String line1;
    String line2;
    bool valid;
    unsigned long lastFetch;
};

// Fetch current crew from Open Notify
bool api_fetchCrew(CrewInfo &crew);

// Fetch TLE from CelesTrak (HTTPS)
bool api_fetchTLE(const char* url, TLEData &tle);
