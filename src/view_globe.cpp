// Globe view — port of preview/index.html `renderOrbitSketch()`.
// Country outlines projected through orthographic camera, city of interest
// marked, ISS + CSS satellites drawn with fading "comet trail" past orbit.

#include "view_globe.h"
#include "display.h"
#include "config.h"
#include "fonts.h"
#include "projection.h"
#include "coastline_data.h"

#include <pgmspace.h>
#include <math.h>

// ── Layout (matches preview SKETCH constants) ─────────────────────────────
static constexpr float EARTH_R   = 60.0f;
static constexpr float EARTH_CX  = DISP_W / 2.0f;
static constexpr float EARTH_CY  = 120.0f;
static constexpr int   STATUS_Y  = DISP_H - 20;
static constexpr float ALT_SCALE = 0.12f; // px per km of altitude
static constexpr int   TRAIL_PTS = 185;   // 92 min × 60 s / 30 s + 1

// Scratch buffer reused for both satellite trails (avoid two 4 KB allocations)
static OrbitTrailPoint trailBuf[TRAIL_PTS];

// City config now lives in state.cities[] — populated from NVS / portal.
// We treat CityEntry directly; no shadow array needed.

// ── Color helpers ─────────────────────────────────────────────────────────
// Fade an RGB565 color toward black by the given alpha (0..1).
// Arduino_GFX has no alpha blending; this is the next best thing on a black bg.
static uint16_t fadeRGB565(uint16_t c, float a) {
    if (a >= 1.0f) return c;
    if (a <= 0.0f) return 0;
    uint8_t r = (c >> 11) & 0x1F;
    uint8_t g = (c >>  5) & 0x3F;
    uint8_t b = (c)       & 0x1F;
    r = (uint8_t)(r * a);
    g = (uint8_t)(g * a);
    b = (uint8_t)(b * a);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// ── Earth disc ────────────────────────────────────────────────────────────
static void drawEarthDisc() {
    gfx->fillCircle((int)EARTH_CX, (int)EARTH_CY, (int)EARTH_R, 0x10A2); // dark navy
    gfx->drawCircle((int)EARTH_CX, (int)EARTH_CY, (int)EARTH_R, 0x4A49); // faint outline
}

// ── Country map ───────────────────────────────────────────────────────────
// Walk PROGMEM coastline data, project each vertex, connect with line where
// both endpoints are visible and inside the disc. Faded blue-grey color.
static void drawCountryMap(const CityEntry& city) {
    constexpr uint16_t COAST_COLOR = 0x2986; // dim slate-blue
    const float er2 = EARTH_R * EARTH_R - 1.0f;

    for (uint16_t r = 0; r < COAST_RING_COUNT; r++) {
        uint16_t start = pgm_read_word(&COAST_RING_OFFSETS[r]);
        uint16_t end   = pgm_read_word(&COAST_RING_OFFSETS[r + 1]);

        bool prevValid = false;
        int  prevX = 0, prevY = 0;

        for (uint16_t v = start; v < end; v++) {
            int16_t latI = (int16_t)pgm_read_word(&COAST_VERTS[v * 2 + 0]);
            int16_t lngI = (int16_t)pgm_read_word(&COAST_VERTS[v * 2 + 1]);
            float lat = (float)latI * 0.01f;
            float lng = (float)lngI * 0.01f;

            ProjPoint p = projection_project(lat, lng, city.centerLat, city.centerLng, EARTH_R);
            if (!p.visible || (p.x * p.x + p.y * p.y) > er2) {
                prevValid = false;
                continue;
            }
            int px = (int)(EARTH_CX + p.x);
            int py = (int)(EARTH_CY + p.y);
            if (prevValid) {
                gfx->drawLine(prevX, prevY, px, py, COAST_COLOR);
            }
            prevX = px; prevY = py;
            prevValid = true;
        }
    }
}

// ── City marker (bright white dot + soft halo via two concentric circles) ─
static void drawCityMarker(const CityEntry& city) {
    ProjPoint p = projection_project(city.lat, city.lng,
                                     city.centerLat, city.centerLng, EARTH_R);
    if (!p.visible) return;
    int px = (int)(EARTH_CX + p.x);
    int py = (int)(EARTH_CY + p.y);
    gfx->fillCircle(px, py, 5, 0x39E7);            // dim halo
    gfx->fillCircle(px, py, 2, COL_TEXT_BRIGHT);   // crisp white core
}

// ── Comet-trail orbit ─────────────────────────────────────────────────────
// pts[0] = current position; pts[N-1] = ~92 min ago. Opacity decreases with
// index → bright behind the satellite, fading toward 0 around the back.
static void drawCometTrail(int satIndex, uint16_t color, const CityEntry& city) {
    int n = orbit_computeTrail(satIndex, trailBuf, TRAIL_PTS);
    if (n < 2) return;

    // Project all points up front so we don't repeat work in the segment loop
    static ProjPoint proj[TRAIL_PTS];
    for (int i = 0; i < n; i++) {
        proj[i] = projection_projectAlt(trailBuf[i].lat, trailBuf[i].lng,
                                        trailBuf[i].alt,
                                        city.centerLat, city.centerLng,
                                        EARTH_R, ALT_SCALE);
    }

    for (int i = 0; i < n - 1; i++) {
        const ProjPoint& a = proj[i];
        const ProjPoint& b = proj[i + 1];
        // Skip wraparound segments where the two adjacent samples land far apart
        if (fabsf(a.x - b.x) > EARTH_R * 1.4f) continue;

        // Comet-trail alpha — bright near the dot (i=0), fading toward the loop tail
        float trailA = powf(1.0f - (float)i / (float)n, 1.4f);
        bool behind  = projection_occluded(a, EARTH_R) || projection_occluded(b, EARTH_R);
        float a_eff  = trailA * (behind ? 0.35f : 1.0f);

        uint16_t segColor = fadeRGB565(color, a_eff);
        if (segColor == 0) continue; // skip near-invisible to save draw calls

        gfx->drawLine(
            (int)(EARTH_CX + a.x), (int)(EARTH_CY + a.y),
            (int)(EARTH_CX + b.x), (int)(EARTH_CY + b.y),
            segColor);
    }
}

// ── Satellite dot at current position ─────────────────────────────────────
static void drawSat(const SatPos& pos, uint16_t color, const CityEntry& city) {
    if (!pos.valid) return;
    ProjPoint p = projection_projectAlt((float)pos.lat, (float)pos.lon, (float)pos.alt,
                                        city.centerLat, city.centerLng,
                                        EARTH_R, ALT_SCALE);
    int cx = (int)(EARTH_CX + p.x);
    int cy = (int)(EARTH_CY + p.y);

    if (projection_occluded(p, EARTH_R)) {
        // Behind Earth — just a small dim marker
        gfx->fillCircle(cx, cy, 3, fadeRGB565(color, 0.45f));
        return;
    }
    // Glow + core
    gfx->fillCircle(cx, cy, 8, color & 0x39E7);
    gfx->fillCircle(cx, cy, 6, color);
    gfx->fillCircle(cx, cy, 3, COL_TEXT_BRIGHT);
}

// ── Simplified legend (toggleable via state.legendsOn) ────────────────────
static void drawLegend(const CityEntry& city) {
    struct Item { const char* label; uint16_t color; };
    Item items[3] = {
        { "ISS",     COL_ISS_DOT },
        { "CSS",     COL_CSS_DOT },
        { city.name, 0xFFFF      }, // bright white
    };

    gfx->setFont(FONT_BODY);
    gfx->setTextSize(1);
    const int legendBaseline = DISP_H - 6;
    int cursorX = 14;
    for (int i = 0; i < 3; i++) {
        // Bright coloured dot
        gfx->fillCircle(cursorX, legendBaseline - 5, 3, items[i].color);
        // Dimmed label (~55% alpha against black bg)
        uint16_t labelColor = fadeRGB565(items[i].color, 0.55f);
        gfx->setTextColor(labelColor);
        gfx->setCursor(cursorX + 8, legendBaseline);
        gfx->print(items[i].label);
        int16_t x1, y1; uint16_t tw, th;
        gfx->getTextBounds(items[i].label, 0, 0, &x1, &y1, &tw, &th);
        cursorX += 8 + tw + 22;
    }
}

// ── Public entry ──────────────────────────────────────────────────────────
void view_globe_render(const SatState &iss, const SatState &css) {
    gfx->fillScreen(COL_BG);

    const CityEntry& city = state.cities[state.currentCity];

    drawEarthDisc();
    drawCountryMap(city);
    drawCityMarker(city);

    // Trails first (so dots draw on top of their tails)
    drawCometTrail(SAT_ISS,      COL_ISS_DOT, city);
    drawCometTrail(SAT_TIANGONG, COL_CSS_DOT, city);
    drawSat(iss.current,         COL_ISS_DOT, city);
    drawSat(css.current,         COL_CSS_DOT, city);

    // Tiny status indicator — only visible WHILE loading
    if (!iss.current.valid || !css.current.valid) {
        char status[24];
        snprintf(status, sizeof(status), "ISS:%s CSS:%s",
                 iss.current.valid ? "OK" : "..", css.current.valid ? "OK" : "..");
        gfx->setFont(FONT_TINY);
        gfx->setTextSize(1);
        gfx->setTextColor(COL_TEXT_DIM);
        int16_t x1, y1; uint16_t tw, th;
        gfx->getTextBounds(status, 0, 0, &x1, &y1, &tw, &th);
        gfx->setCursor(DISP_W - (int)tw - 4, 12);
        gfx->print(status);
    }

    if (state.legendsOn) drawLegend(city);
}
