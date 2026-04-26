#include "view_orbit.h"
#include "display.h"
#include "config.h"
#include <math.h>
#include <stdio.h>

// Earth-centered orbital diagram

#define EARTH_R     35   // Earth radius
#define EARTH_CX    (DISP_W / 2)
#define EARTH_CY    ((DISP_H - 22) / 2)
#define STATUS_Y    (DISP_H - 20)

// Orbit radii
#define ISS_ORBIT_R    90
#define CSS_ORBIT_R    75

// Get point on tilted orbit ellipse
static void orbitPoint(int cx, int cy, int radius, float incDeg, float angleDeg, int &ox, int &oy) {
    float incRad = incDeg * M_PI / 180.0f;
    float angRad = angleDeg * M_PI / 180.0f;
    ox = cx + (int)(radius * cosf(angRad));
    oy = cy + (int)(radius * sinf(angRad) * cosf(incRad));
}

// Check if behind Earth (in the lower arc of the ellipse)
static bool isBehindEarth(int cx, int cy, int radius, float incDeg, float angleDeg) {
    float angRad = angleDeg * M_PI / 180.0f;
    if (sinf(angRad) <= 0.2f) return false;  // front half
    int x, y;
    orbitPoint(cx, cy, radius, incDeg, angleDeg, x, y);
    return (abs(x - cx) < EARTH_R + 2) && (abs(y - cy) < EARTH_R + 2);
}

// Draw thick orbit ring (draw 3 parallel lines for thickness)
static void drawOrbitRing(int cx, int cy, int radius, float incDeg, uint16_t color) {
    for (int offset = -1; offset <= 1; offset++) {
        int r = radius + offset;
        int prevX = -1, prevY = -1;
        for (int deg = 0; deg <= 360; deg += 2) {
            int x, y;
            orbitPoint(cx, cy, r, incDeg, (float)deg, x, y);

            if (prevX >= 0) {
                bool behind = isBehindEarth(cx, cy, radius, incDeg, (float)deg);
                if (!behind) {
                    gfx->drawLine(prevX, prevY, x, y, color);
                } else {
                    // Draw dimmer behind Earth
                    uint16_t dim = ((color >> 1) & 0x7BEF);
                    dim = ((dim >> 1) & 0x7BEF);
                    gfx->drawLine(prevX, prevY, x, y, dim);
                }
            }
            prevX = x;
            prevY = y;
        }
    }
}

// Compute orbit angle from satellite longitude
static float satToOrbitAngle(const SatPos &pos) {
    if (!pos.valid) return 0;
    return fmod(pos.lon + 180.0f, 360.0f);
}

// Draw satellite as a bright dot — always visible (no occlusion culling)
static void drawSatDot(int cx, int cy, int radius, float incDeg,
                       float angleDeg, uint16_t dotColor) {
    int x, y;
    orbitPoint(cx, cy, radius, incDeg, angleDeg, x, y);

    // Outer glow
    gfx->fillCircle(x, y, 8, dotColor & 0x39E7);
    // Mid ring
    gfx->fillCircle(x, y, 6, dotColor);
    // Bright core
    gfx->fillCircle(x, y, 3, COL_TEXT_BRIGHT);
}

// Draw Earth — dark fill with clear white outline
static void drawEarth(int cx, int cy, int r) {
    // Dark filled circle
    gfx->fillCircle(cx, cy, r, 0x1928);

    // Clear white outline (2px thick)
    gfx->drawCircle(cx, cy, r, COL_TEXT_BRIGHT);
    gfx->drawCircle(cx, cy, r + 1, COL_TEXT_BRIGHT);
}

void view_orbit_render(const SatState &iss, const SatState &tiangong, bool wifiConnected) {
    gfx->fillScreen(COL_BG);

    // ── Earth ──
    drawEarth(EARTH_CX, EARTH_CY, EARTH_R);

    // ── Orbit rings — thick, clear ──
    drawOrbitRing(EARTH_CX, EARTH_CY, ISS_ORBIT_R, 51.6f, COL_ISS_TRACK);
    drawOrbitRing(EARTH_CX, EARTH_CY, CSS_ORBIT_R, 41.5f, COL_CSS_TRACK);

    // ── Satellite dots ──
    if (iss.current.valid) {
        float angle = satToOrbitAngle(iss.current);
        drawSatDot(EARTH_CX, EARTH_CY, ISS_ORBIT_R, 51.6f, angle, COL_ISS_DOT);
    }

    if (tiangong.current.valid) {
        float angle = satToOrbitAngle(tiangong.current);
        drawSatDot(EARTH_CX, EARTH_CY, CSS_ORBIT_R, 41.5f, angle, COL_CSS_DOT);
    }

    // Tiny status indicator — only visible WHILE loading. Hides once both OK.
    if (!iss.current.valid || !tiangong.current.valid) {
        char status[24];
        snprintf(status, sizeof(status), "ISS:%s CSS:%s",
                 iss.current.valid ? "OK" : "..", tiangong.current.valid ? "OK" : "..");
        gfx->setFont(FONT_TINY);
        gfx->setTextSize(1);
        gfx->setTextColor(COL_TEXT_DIM);
        int16_t x1, y1; uint16_t tw, th;
        gfx->getTextBounds(status, 0, 0, &x1, &y1, &tw, &th);
        gfx->setCursor(DISP_W - (int)tw - 4, 12);
        gfx->print(status);
    }

    if (!iss.current.valid && !tiangong.current.valid) {
        gfx->setFont(FONT_TINY);
        gfx->setTextSize(1);
        gfx->setTextColor(COL_TEXT_DIM);
        int16_t x1, y1;
        uint16_t tw, th;
        const char* msg = "Waiting for orbit data...";
        gfx->getTextBounds(msg, 0, 0, &x1, &y1, &tw, &th);
        gfx->setCursor((DISP_W - tw) / 2, 16);
        gfx->print(msg);
    }

    // ── Status bar — TINY font fits the full ISS + CSS coords on one line.
    // Inter Body would overlap because the data is too dense for half-width.
    gfx->setFont(FONT_TINY);
    gfx->setTextSize(1);

    const int statusBaseline = DISP_H - 6;

    if (iss.current.valid) {
        char buf[50];
        snprintf(buf, sizeof(buf), "ISS  %.1f%c %.1f%c  %dkm",
                 fabs(iss.current.lat), iss.current.lat >= 0 ? 'N' : 'S',
                 fabs(iss.current.lon), iss.current.lon >= 0 ? 'E' : 'W',
                 (int)iss.current.alt);
        gfx->setTextColor(COL_ISS_DOT);
        gfx->setCursor(12, statusBaseline);
        gfx->print(buf);
        gfx->fillCircle(6, statusBaseline - 4, 3, COL_ISS_DOT);
    }

    if (tiangong.current.valid) {
        char buf[50];
        snprintf(buf, sizeof(buf), "CSS  %.1f%c %.1f%c  %dkm",
                 fabs(tiangong.current.lat), tiangong.current.lat >= 0 ? 'N' : 'S',
                 fabs(tiangong.current.lon), tiangong.current.lon >= 0 ? 'E' : 'W',
                 (int)tiangong.current.alt);
        gfx->setTextColor(COL_CSS_DOT);
        gfx->setCursor(DISP_W / 2 + 8, statusBaseline);
        gfx->print(buf);
        gfx->fillCircle(DISP_W / 2 + 2, statusBaseline - 4, 3, COL_CSS_DOT);
    }

    // WiFi indicator
    if (!wifiConnected) {
        gfx->fillCircle(DISP_W - 8, 6, 3, COL_WIFI_ERR);
    }
}
