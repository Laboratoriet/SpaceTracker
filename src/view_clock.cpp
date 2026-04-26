// Clock view — big 24h time + date, optional starfield background.
// Mirrors preview/index.html `renderClock()`.

#include "view_clock.h"
#include "display.h"
#include "config.h"
#include "fonts.h"
#include "state.h"

#include <Arduino.h>
#include <time.h>

// ── Starfield ─────────────────────────────────────────────────────────────
// Four brightness tiers mirror the preview generator: lots of faint stars,
// fewer mid, fewer bright, a handful of "hero" 2x2 stars.
struct Star {
    uint16_t x, y;
    uint8_t  alpha;  // 46, 115, 217, 255 (rounded from 0.18, 0.45, 0.85, 1.0)
    uint8_t  size;   // 1 or 2
};

static constexpr int STAR_COUNT = 90 + 45 + 18 + 6;
static Star stars[STAR_COUNT];
static uint16_t starColors[STAR_COUNT]; // pre-blended RGB565 for fast draw

static uint32_t seedRng = 7919;
static float nextRand() {
    seedRng = (seedRng * 1103515245u + 12345u) & 0x7fffffffu;
    return (float)seedRng / (float)0x7fffffff;
}

static uint16_t whiteFade(uint8_t alpha) {
    // White (R=31, G=63, B=31) scaled by alpha/255.
    uint8_t r = (31 * alpha) / 255;
    uint8_t g = (63 * alpha) / 255;
    uint8_t b = (31 * alpha) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void view_clock_initStars() {
    seedRng = 7919; // deterministic — same field every boot
    struct Tier { int count; uint8_t alpha; uint8_t size; };
    Tier tiers[4] = {
        { 90, 46,  1 }, // very faint background
        { 45, 115, 1 }, // mid
        { 18, 217, 1 }, // bright
        {  6, 255, 2 }, // hero
    };
    int idx = 0;
    for (int t = 0; t < 4; t++) {
        for (int i = 0; i < tiers[t].count; i++) {
            stars[idx].x     = (uint16_t)(nextRand() * DISP_W);
            stars[idx].y     = (uint16_t)(nextRand() * DISP_H);
            stars[idx].alpha = tiers[t].alpha;
            stars[idx].size  = tiers[t].size;
            starColors[idx]  = whiteFade(tiers[t].alpha);
            idx++;
        }
    }
}

static void drawStarfield() {
    for (int i = 0; i < STAR_COUNT; i++) {
        if (stars[i].size == 1) {
            gfx->drawPixel(stars[i].x, stars[i].y, starColors[i]);
        } else {
            gfx->fillRect(stars[i].x, stars[i].y, 2, 2, starColors[i]);
        }
    }
}

// ── Main render ───────────────────────────────────────────────────────────
void view_clock_render() {
    gfx->fillScreen(COL_BG);

    if (state.starsOn) drawStarfield();

    time_t now;
    time(&now);
    struct tm tinfo;
    localtime_r(&now, &tinfo);

    // Big time HH:MM (Inter Black 92, digits + colon glyphs only)
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tinfo.tm_hour, tinfo.tm_min);

    gfx->setFont(FONT_BIG_NUM);
    gfx->setTextSize(1);
    gfx->setTextColor(COL_TEXT_BRIGHT);

    int16_t x1, y1; uint16_t tw, th;
    gfx->getTextBounds(timeStr, 0, 0, &x1, &y1, &tw, &th);

    int blockH = th + 30 + 16;
    int startY = (DISP_H - blockH) / 2 + th;
    gfx->setCursor((DISP_W - tw) / 2, startY);
    gfx->print(timeStr);

    // Date subtitle, e.g. "Sun, 26 April 2026"
    char dateStr[40];
    strftime(dateStr, sizeof(dateStr), "%a, %e %B %Y", &tinfo);
    // Collapse the leading space %e produces for single-digit days, plus the
    // resulting double-space after the comma.
    char* dbl = strstr(dateStr, ",  ");
    if (dbl) memmove(dbl + 2, dbl + 3, strlen(dbl + 3) + 1);

    // Switch to a font that has letters (the BIG_NUM glyph set is digits only)
    gfx->setFont(FONT_SUBTITLE);
    display_drawCentered(startY + 40, dateStr, COL_TEXT_DIM);
}
