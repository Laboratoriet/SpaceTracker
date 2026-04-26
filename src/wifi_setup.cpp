// First-boot WiFi flow with captive portal. Built directly on the ESP32
// Arduino core — no external dependencies (no WiFiManager, etc.).
//
//  - Loads SSID/pass from `state.wifiSsid` / `state.wifiPass`
//  - If empty OR STA connect fails after ~20s → switch to AP mode, start
//    a captive DNS server (all hostnames → 192.168.4.1), serve a setup
//    HTML form on /, save submitted creds to NVS, ESP.restart()
//
// While in portal mode, `wifi_setup_loop()` services the DNS + HTTP
// servers each iteration. Buttons / views / data fetches should not run.

#include "wifi_setup.h"

#include "state.h"
#include "display.h"
#include "config.h"
#include "view_splash.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static bool        portalMode = false;
static DNSServer   dnsServer;
static WebServer   apServer(80);

// AP IP (default for SoftAP)
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_NET(255, 255, 255, 0);

// ── HTML for the AP setup page ────────────────────────────────────────────
// Self-contained — embedded as a raw string. Renders a styled form for
// SSID picker, password, and submit. JS pre-populates the SSID list from
// /scan endpoint (one-shot WiFi scan in AP mode).
static const char SETUP_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SpaceTracker WiFi setup</title>
<style>
  body{font:16px/1.5 -apple-system,system-ui,sans-serif;margin:0;background:#0a0a0b;color:#e6e6ea;padding:32px 20px;max-width:480px;margin:auto}
  h1{font-size:20px;margin:0 0 4px}
  p.sub{color:#8a8a92;margin:0 0 24px;font-size:13px}
  label{display:block;margin:18px 0 6px;font-size:13px;color:#aaa}
  input,select{width:100%;padding:11px 12px;font:inherit;background:#16161a;color:inherit;border:1px solid #26262c;border-radius:8px;box-sizing:border-box}
  input:focus,select:focus{outline:none;border-color:#3b82f6}
  button{margin-top:24px;width:100%;padding:13px;font:600 15px/1 inherit;background:#3b82f6;color:#fff;border:0;border-radius:8px;cursor:pointer}
  button:hover{background:#2563eb}
  button:disabled{opacity:.5;cursor:not-allowed}
  .status{margin-top:18px;padding:10px 12px;background:#16161a;border-radius:6px;font-size:13px;color:#aaa}
  .err{color:#ff6b6b}
  .row{display:flex;gap:8px}
  .row select{flex:1}
  .row button{margin-top:0;flex:0 0 auto;width:auto;padding:11px 14px;background:#26262c}
</style>
</head><body>
<h1>SpaceTracker</h1>
<p class="sub">Connect this device to your home WiFi to get started.</p>

<form id="f" method="POST" action="/save">
  <label for="ssid">Network</label>
  <div class="row">
    <select name="ssid" id="ssid" required>
      <option value="">Loading networks…</option>
    </select>
    <button type="button" id="rescan">Rescan</button>
  </div>

  <label for="pw">Password</label>
  <input type="password" name="pw" id="pw" autocomplete="off" placeholder="(leave blank for open networks)">

  <label for="tz">Timezone offset (hours from UTC)</label>
  <select name="tz" id="tz">
    <option value="-12">UTC −12</option><option value="-11">UTC −11</option>
    <option value="-10">UTC −10</option><option value="-9">UTC −9</option>
    <option value="-8">UTC −8 (Pacific)</option><option value="-7">UTC −7</option>
    <option value="-6">UTC −6 (Central)</option><option value="-5">UTC −5 (Eastern)</option>
    <option value="-4">UTC −4</option><option value="-3">UTC −3</option>
    <option value="-2">UTC −2</option><option value="-1">UTC −1</option>
    <option value="0">UTC 0 (London)</option>
    <option value="1" selected>UTC +1 (Berlin / Oslo)</option>
    <option value="2">UTC +2 (Helsinki / Athens)</option>
    <option value="3">UTC +3 (Moscow)</option><option value="4">UTC +4</option>
    <option value="5">UTC +5</option><option value="6">UTC +6</option>
    <option value="7">UTC +7</option><option value="8">UTC +8</option>
    <option value="9">UTC +9 (Tokyo)</option><option value="10">UTC +10</option>
    <option value="11">UTC +11</option><option value="12">UTC +12</option>
  </select>
  <label style="display:flex;gap:8px;align-items:center;margin-top:14px">
    <input type="checkbox" name="dst" value="1" checked style="width:auto">
    <span style="color:#aaa">Apply daylight saving (+1h in summer)</span>
  </label>

  <button type="submit" id="go">Save &amp; restart</button>
  <div class="status" id="s">Once you submit, the device will restart and join the network you picked. Look for the IP address on the SpaceTracker screen, then visit it from any browser to keep configuring (cities, views, etc.).</div>
</form>

<script>
async function scan(){
  const sel=document.getElementById('ssid');
  sel.innerHTML='<option value="">Scanning…</option>';
  try{
    const r=await fetch('/scan'); const j=await r.json();
    if(!j.length){sel.innerHTML='<option value="">No networks found</option>';return;}
    sel.innerHTML=j.map(n=>`<option value="${n.s}">${n.s} ${'▮'.repeat(Math.min(4,Math.max(1,Math.round((n.r+90)/15))))}</option>`).join('');
  }catch(e){
    sel.innerHTML='<option value="">Scan failed — type SSID below</option>';
    sel.outerHTML=sel.outerHTML.replace('select','input').replace('</select>','').replace('Scan failed — type SSID below','');
  }
}
document.getElementById('rescan').addEventListener('click',scan);
scan();
</script>
</body></html>
)HTML";

// ── Captive-portal probe handlers ─────────────────────────────────────────
// Different OSes hit different probe URLs. Returning a 302 to our root makes
// them all pop up the captive notification on iOS / Android / macOS / Windows.
static void redirectToRoot() {
    apServer.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
    apServer.send(302, "text/plain", "");
}

static void handleSetupPage() {
    apServer.send_P(200, "text/html", SETUP_PAGE);
}

static void handleScan() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i) json += ",";
        String ssid = WiFi.SSID(i);
        ssid.replace("\"", "\\\"");
        json += "{\"s\":\"" + ssid + "\",\"r\":" + WiFi.RSSI(i) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    apServer.sendHeader("Cache-Control", "no-store");
    apServer.send(200, "application/json", json);
}

static void handleSave() {
    String ssid = apServer.arg("ssid");
    String pw   = apServer.arg("pw");
    String tz   = apServer.arg("tz");
    String dst  = apServer.arg("dst");

    if (ssid.length() == 0 || ssid.length() >= sizeof(state.wifiSsid)) {
        apServer.send(400, "text/plain", "SSID required");
        return;
    }
    if (pw.length() >= sizeof(state.wifiPass)) {
        apServer.send(400, "text/plain", "Password too long");
        return;
    }

    strncpy(state.wifiSsid, ssid.c_str(), sizeof(state.wifiSsid) - 1);
    state.wifiSsid[sizeof(state.wifiSsid) - 1] = 0;
    strncpy(state.wifiPass, pw.c_str(), sizeof(state.wifiPass) - 1);
    state.wifiPass[sizeof(state.wifiPass) - 1] = 0;
    if (tz.length())  state.ntpOffsetHours = (int8_t)tz.toInt();
    state.ntpDstHours = dst == "1" ? 1 : 0;
    state_save();

    apServer.send(200, "text/html",
                  "<h2>Saved. Restarting…</h2>"
                  "<p>The device will reboot and try to join \"" + ssid + "\".</p>");

    delay(800);
    Serial.println("WiFi creds saved — restarting");
    ESP.restart();
}

static void startCaptivePortal() {
    portalMode = true;

    // Build the AP SSID with last 4 hex digits of MAC for uniqueness
    uint8_t mac[6]; WiFi.macAddress(mac);
    char apSsid[32];
    snprintf(apSsid, sizeof(apSsid), "SpaceTracker-%02X%02X", mac[4], mac[5]);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_NET);
    WiFi.softAP(apSsid);  // open network — easier first-time
    Serial.printf("AP started: %s @ %s\n", apSsid, WiFi.softAPIP().toString().c_str());

    // Captive DNS — every hostname → us
    dnsServer.start(53, "*", AP_IP);

    // Common OS captive-portal probe URLs → redirect to our setup page
    apServer.on("/",                       handleSetupPage);
    apServer.on("/save",          HTTP_POST, handleSave);
    apServer.on("/scan",                   handleScan);
    apServer.on("/generate_204",           redirectToRoot); // Android
    apServer.on("/gen_204",                redirectToRoot); // Android
    apServer.on("/hotspot-detect.html",    redirectToRoot); // iOS / macOS
    apServer.on("/library/test/success.html", redirectToRoot); // iOS
    apServer.on("/connecttest.txt",        redirectToRoot); // Windows
    apServer.on("/ncsi.txt",               redirectToRoot); // Windows
    apServer.onNotFound(redirectToRoot);

    apServer.begin();

    // Tell the user what to do, on the AMOLED itself
    char line2[64];
    snprintf(line2, sizeof(line2), "Join %s", apSsid);
    view_splash_render(line2);
    delay(2000);
    view_splash_render("Open setup at 192.168.4.1");
}

bool wifi_setup_connect_or_portal() {
    // No saved creds → straight to portal
    if (state.wifiSsid[0] == 0) {
        startCaptivePortal();
        return false;
    }

    Serial.printf("Trying saved WiFi: %s\n", state.wifiSsid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(state.wifiSsid, state.wifiPass);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("\nFailed to connect to saved WiFi — entering portal");
    startCaptivePortal();
    return false;
}

void wifi_setup_loop() {
    if (!portalMode) return;
    dnsServer.processNextRequest();
    apServer.handleClient();
}

bool wifi_setup_in_portal_mode() { return portalMode; }
