#pragma once

#include <stdint.h>

// Boot-time WiFi flow:
//  - if state.wifiSsid is empty OR connect attempt fails, start AP mode and
//    serve a captive setup page on http://192.168.4.1/
//  - on submit, save creds to NVS and reboot
//  - on successful STA connect, return true and let main() proceed
//
// Calling code (main.cpp setup()) is expected to render its own splash before
// and after, but wifi_setup will render its own AP-mode UI on the AMOLED via
// view_splash to tell the user what to do.
bool wifi_setup_connect_or_portal();

// Called every loop iteration when in AP/portal mode. No-op when STA-connected.
void wifi_setup_loop();

// True when the device is currently in AP/captive-portal mode (i.e. the user
// hasn't finished WiFi setup yet and the regular features should be skipped).
bool wifi_setup_in_portal_mode();
