# Troubleshooting

If something on SpaceTracker isn't working, scan this list before opening
an issue. Most surprises are documented here from real builds.

---

## Display problems

### Screen is dim and hard to read

The default brightness on the AMOLED is intentionally moderate to extend
panel life. Press **B2** (button 2) to cycle brightness Low → Medium →
High, or set it via the web portal under "Display & controls". Settings
persist across reboots.

### Screen flashes / flickers when content updates

Should not happen — the firmware uses canvas double-buffering in PSRAM
and pushes whole frames to the AMOLED in one shot. If you see flicker:

1. Verify your build includes the canvas (`grep "Arduino_Canvas" src/display.cpp`)
2. Check your board has PSRAM (`build_flags = -DBOARD_HAS_PSRAM` in `platformio.ini`)
3. Confirm `board_build.arduino.memory_type = qio_opi`

### Screen is completely black

- Brightness might be at zero — long-press **B2** to wake / cycle.
- Power-cycle the board (unplug + replug USB).
- If still black: re-flash the firmware. **NEVER write `firmware.bin` to
  address `0x0`** — that overwrites the bootloader and bricks the board.
  Correct addresses: `0x0 bootloader.bin`, `0x8000 partitions.bin`,
  `0x10000 firmware.bin`.

### Display stays blank but the green LED blinks at boot

If you see the LED blink 4 times rapidly at boot but the screen never lights
up, you likely added a new view/splash render somewhere and forgot to call
`display_flush()` afterwards. Every render writes to the PSRAM canvas; only
`display_flush()` actually pushes the canvas to the AMOLED.

`view_splash_render()` auto-flushes since v1.0.1, but the per-view renders
in `renderCurrent()` rely on `main.cpp`'s loop calling `display_flush()`
exactly once per dirty cycle. If you add a render path outside that loop,
remember to flush yourself.

### Display goes dark when toggling the LED

If the display blanks when you long-press B2, your board may be a
revision where GPIO 38 is shared with display power. Mitigation: edit
`src/display.cpp` and remove the `digitalWrite(PIN_LED, ...)` from the
`applyLed()` chain. The user LED is documented as GPIO 38 on the
**non-touch** variant; touch boards may differ.

---

## WiFi & network

### Can't see the `SpaceTracker-XXXX` AP on first boot

- Wait 15-20 seconds after power-on — WiFi takes a moment to come up.
- The AP is on 2.4 GHz only (ESP32 limitation). Make sure your phone
  isn't filtered to 5 GHz.
- If still missing, hold both buttons (or use the webportal "Reset to
  defaults") to wipe NVS and start the captive portal fresh.

### Captive portal page doesn't appear after joining the AP

Some OSes only show the captive notification when the device explicitly
fails their connectivity check. SpaceTracker handles the common probe
URLs (`/generate_204`, `/hotspot-detect.html`, `/ncsi.txt`, etc.), but if
yours doesn't pop up, manually browse to **http://192.168.4.1/**.

### "WiFi failed" after submitting credentials

- Double-check the password (typos are common). Tip: type it into a note
  app first, then paste.
- Verify the network is **2.4 GHz**. ESP32-S3 doesn't support 5 GHz.
- Some routers block clients without an internet route during captive
  portal redirects. Disable any "client isolation" or "guest network"
  rules.

### Device won't reconnect after my router restarts

- The reconnect logic retries every 30s. Wait 1-2 minutes.
- If still not connecting, the router may have changed channels in a way
  the ESP32 didn't redetect. Power-cycle the device.

### Can't reach `spacetracker.local`

- mDNS isn't supported on every OS. Try the IP address directly — it's
  shown briefly on the splash screen at boot, or under any view via
  long-press B1+B2 (info overlay).
- On Windows, install the optional "Bonjour" service (comes with iTunes
  or Apple's separate Bonjour Print Services).
- On Linux, ensure `avahi-daemon` is running.

---

## Data & TLS

### Status indicator shows `ISS:.. CSS:..` for a long time

The remote TLE fetch from CelesTrak failed. The hardcoded fallback TLEs
in `src/tle_fallback.h` should still drive the satellites, so this is
mostly cosmetic. Live retry happens every 6 hours.

### `[E][WiFiClientSecure.cpp:144] connect(): start_ssl_client: -1`

Common ESP32 TLS-handshake failure. Causes & fixes:

1. **Stale TLS state on a reused client.** `src/api.cpp` already creates
   a fresh `WiFiClientSecure` per request — don't switch back to a
   cached singleton.
2. **Server rate-limit / firewall.** CelesTrak blocks IPs that hammer
   their servers. SpaceTracker refreshes TLEs every 6 h to be polite.
3. **Out of heap during handshake.** Free PSRAM by reducing canvas use
   (unlikely to help) or check for memory leaks.

### CelesTrak `403 Forbidden` — "excessive downloads"

Your IP is rate-limited for 2 hours. Wait it out. The hardcoded TLE
fallback keeps everything working. Don't reduce `TLE_REFRESH_MS` below
the default 6 h.

### Daylight tracker shows a night message during the day

NTP timezone is wrong. Open the web portal → "WiFi & Time" → set
"Timezone offset" to your UTC offset. SpaceTracker stores time in UTC
internally (so the on-device clock and sun position computations are
correct) and applies the offset only when formatting strings.

### Crew count says "Fetching data…" forever

Open Notify (`api.open-notify.org/astros.json`) is HTTP-only and
sometimes flaky. The firmware retries every 30 s until first success,
then every 5 min. If it never succeeds:

- Verify HTTP egress isn't blocked by your network.
- Hit the URL from your computer to confirm the API is up.
- The view shows "Fetching data…" only until the first successful
  response — once seen, even stale data is shown.

---

## Build & flash

### "Port busy" when flashing

You have `pio device monitor` open on the same port. Close it
(Ctrl+C) and re-flash.

### `pio run` complains about a missing library

Run `pio pkg install` (or just re-run `pio run` — PlatformIO will fetch
deps automatically). Required libs are listed in `platformio.ini`:

```ini
lib_deps =
    mathertel/OneButton
    bblanchon/ArduinoJson
    hopperpop/Sgp4
    olikraus/U8g2
```

### Flash succeeds but the device doesn't boot

- Verify you're using `--no-stub` and the correct addresses (`0x0`,
  `0x8000`, `0x10000`). Skipping `--no-stub` causes the QSPI boot to
  hang on this board.
- Power-cycle (unplug + replug USB). The RTS-pin reset doesn't always
  come up cleanly.

### "Esptool can't find chip"

Hold the **boot button** (B1, GPIO 0) while plugging in the USB cable.
That forces the ESP32-S3 into download mode. Release the button after
the flash starts.

---

## Web portal

### Settings page won't load

- Confirm you're on the same WiFi network as the device.
- Check the IP shown on the splash screen at boot.
- Try `http://<ip>/` and `http://spacetracker.local/`.

### "Save" button does nothing

- Open the browser console (F12) — look for fetch errors.
- Reload the page. Some changes (WiFi, NTP) trigger an immediate reboot
  and the response may not arrive before the device restarts.

### Cities, but the globe view doesn't show them

The globe view cycles through cities via **B1 double-click**. After
adding a city in the portal, double-click B1 on the globe view to step
through them.

### "Reset to defaults" wiped my WiFi

Yes — that's intentional. After a reset, the device returns to first-
boot mode (captive portal). Re-join `SpaceTracker-XXXX` from your phone
to re-configure.

---

## Hardware notes specific to LilyGo T-Display S3 AMOLED

- **GPIO 38 is the green status LED on the non-touch variant.** Older
  examples named it `PIN_POWER_ON` and held it HIGH always — that was
  misleading; the side effect was just keeping the LED lit. The display
  has its OWN power and does NOT need GPIO 38 HIGH.
- **The touch variant** likely uses GPIO 38 for something else. Verify
  before toggling on a different board.
- **Display rotation 1** = landscape 536×240. Rotation 0 is portrait
  240×536.
- **Brightness register** is `0x51` — `bus->writeC8D8(0x51, value)` where
  value is 0-255. Practical: low ≈ 80, mid ≈ 180, high ≈ 255.
