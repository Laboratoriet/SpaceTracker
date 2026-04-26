#pragma once

// Web config portal — reachable at http://<ip>/ or http://spacetracker.local/
// once the device has joined a WiFi network. Hosts a single-page settings UI
// + a JSON API for programmatic control.
//
//   GET  /              → settings UI
//   GET  /api/state     → JSON of full State struct
//   POST /api/state     → update fields, persist, apply live (or schedule reboot)
//   GET  /api/status    → uptime, RSSI, NTP, last-fetch ages
//   POST /api/reset     → wipe NVS, reboot
//   POST /api/restart   → reboot (no NVS wipe)
//
// `webportal_begin()` starts the WebServer on port 80 and registers mDNS
// hostname `spacetracker.local`. `webportal_loop()` services pending HTTP
// requests — call every loop iteration.

void webportal_begin();
void webportal_loop();
