// Daylight tracker view — port of preview/index.html `renderDaylight()`.
//
// Inspired by bakkenbaeck/daylight-ios. Uses a continuous color gradient
// based on the actual sun altitude rather than discrete phases.

#include "view_daylight.h"
#include "display.h"
#include "config.h"
#include "fonts.h"
#include "state.h"
#include "suncalc.h"

#include <Arduino.h>
#include <math.h>
#include <time.h>

// ── 24-bit RGB stops, kept as packed uint32_t for compact arrays ──────────
static inline uint32_t RGB(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline uint16_t toRGB565(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >>  8) & 0xFF;
    uint8_t b = (rgb)       & 0xFF;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
static uint32_t lerpRGB(uint32_t a, uint32_t b, float t) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    int r = ar + (int)((br - ar) * t);
    int g = ag + (int)((bg - ag) * t);
    int bl = ab + (int)((bb - ab) * t);
    return RGB((uint8_t)r, (uint8_t)g, (uint8_t)bl);
}

// ── Color gradient stops (mirrors preview DAYLIGHT_STOPS_*) ───────────────
struct ColorStop { float altDeg; uint32_t bg; uint32_t text; };

static const ColorStop STOPS_MORNING[] = {
    { -25.0f, RGB(0x06,0x13,0x1F), RGB(0x90,0xCF,0xEF) },
    { -12.0f, RGB(0x0F,0x1E,0x36), RGB(0x7A,0xA8,0xD0) },
    {  -6.0f, RGB(0x3D,0x5C,0x8C), RGB(0xC5,0xD5,0xEE) },
    {  -2.0f, RGB(0x9D,0xB8,0xDC), RGB(0x50,0x65,0x90) },
    {   0.0f, RGB(0xFD,0xED,0xA8), RGB(0xDB,0x60,0x28) },
    {   4.0f, RGB(0xFB,0xE5,0xAA), RGB(0xC4,0x52,0x30) },
    {  12.0f, RGB(0xFB,0xE0,0xAA), RGB(0xA0,0x4C,0x2C) },
    {  25.0f, RGB(0xFA,0xDD,0xA4), RGB(0x9A,0x48,0x28) },
    {  45.0f, RGB(0xF8,0xDE,0xB6), RGB(0x8C,0x3F,0x1F) },
};
static const ColorStop STOPS_EVENING[] = {
    {  45.0f, RGB(0xF8,0xDE,0xB6), RGB(0x8C,0x3F,0x1F) },
    {  25.0f, RGB(0xFA,0xDD,0xA4), RGB(0x9A,0x48,0x28) },
    {  12.0f, RGB(0xF8,0xD2,0xA8), RGB(0xA0,0x4C,0x2C) },
    {   4.0f, RGB(0xF6,0xC5,0xA2), RGB(0xA0,0x4C,0x2C) },
    {   0.0f, RGB(0xF7,0xC5,0xB1), RGB(0xA0,0x4C,0x2C) },
    {  -2.0f, RGB(0xB5,0x9C,0xB0), RGB(0x5A,0x46,0x70) },
    {  -6.0f, RGB(0x5C,0x7A,0xA8), RGB(0xD3,0xE5,0xFD) },
    { -12.0f, RGB(0x1F,0x2D,0x4D), RGB(0x95,0xB5,0xD9) },
    { -25.0f, RGB(0x06,0x13,0x1F), RGB(0x90,0xCF,0xEF) },
};

struct Theme { uint16_t bg; uint16_t text; };

static Theme gradientColors(float altDeg, bool isMorning) {
    const ColorStop* stops = isMorning ? STOPS_MORNING : STOPS_EVENING;
    int n = isMorning
              ? (int)(sizeof(STOPS_MORNING) / sizeof(STOPS_MORNING[0]))
              : (int)(sizeof(STOPS_EVENING) / sizeof(STOPS_EVENING[0]));

    for (int i = 0; i < n - 1; i++) {
        float a1 = stops[i].altDeg, a2 = stops[i + 1].altDeg;
        float lo = a1 < a2 ? a1 : a2;
        float hi = a1 > a2 ? a1 : a2;
        if (altDeg >= lo && altDeg <= hi) {
            float t = (altDeg - a1) / (a2 - a1);
            uint32_t bg   = lerpRGB(stops[i].bg,   stops[i + 1].bg,   t);
            uint32_t text = lerpRGB(stops[i].text, stops[i + 1].text, t);
            return { toRGB565(bg), toRGB565(text) };
        }
    }
    // Out of range — clamp to nearest end
    int idx = (altDeg < stops[0].altDeg) ? 0 : (n - 1);
    if (isMorning && altDeg > stops[n-1].altDeg) idx = n - 1;
    return { toRGB565(stops[idx].bg), toRGB565(stops[idx].text) };
}

// ── Duration helpers ──────────────────────────────────────────────────────
static String fmtDeltaWords(long deltaSec, bool isLonger) {
    long absSec = deltaSec < 0 ? -deltaSec : deltaSec;
    if (absSec < 30) return isLonger ? "a touch more sun" : "a touch less sun";
    long min = (absSec + 30) / 60;
    if (min < 1) min = 1;
    String s = String((long)min);
    s += isLonger ? " more " : " fewer ";
    s += (min == 1 ? "minute" : "minutes");
    return s;
}

static String fmtHM(time_t t) {
    struct tm tinfo;
    localtime_r(&t, &tinfo);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", tinfo.tm_hour, tinfo.tm_min);
    return String(buf);
}

// ── Message templates (subset of daylight-ios Message.swift) ──────────────
static const char* MSG_DAY_LONGER[] = {
    "**{d}** of daylight today. Make them count!",
    "Today has **{d}** of sunshine. Enjoy!",
    "Smile - today has **{d}** of daylight than yesterday.",
    "Bring out your shorts, today has **{d}** of sunlight.",
    "**{d}** of sun today. Soak it up!",
};
static const char* MSG_DAY_SHORTER[] = {
    "**{d}** of sun today. Keep your head up!",
    "Sadly, today has **{d}** of sunlight. Make the most of it.",
    "Today is a bit shorter - **{d}** of light than yesterday.",
};
static const char* MSG_NIGHT_LONGER[] = {
    "Get a good night's sleep - **{d}** of sunlight tomorrow.",
    "Bring out those pyjamas. **{d}** of light awaits tomorrow.",
    "**{d}** of extra daylight awaits tomorrow.",
};
static const char* MSG_NIGHT_SHORTER[] = {
    "Tomorrow has **{d}** of sunlight than today. Make the most of it.",
    "Brighter times ahead - but tomorrow has **{d}** of light first.",
};

// Tiny-delta templates (|delta| < 30 sec, near the solstices). These are
// FULL sentences with no {d} slot — they don't try to cram a noun phrase in.
static const char* MSG_TINY_DAY_LONGER[] = {
    "Today has just a touch more sunlight than yesterday.",
    "We're at the tipping point - days start getting longer now.",
    "Just a touch brighter today. Soak it in!",
};
static const char* MSG_TINY_DAY_SHORTER[] = {
    "Today is just slightly shorter than yesterday.",
    "We've hit the longest day. From here, sun fades a little each day.",
    "Just a touch less light today. Make it count.",
};
static const char* MSG_TINY_NIGHT_LONGER[] = {
    "Tomorrow will have just a touch more sunlight.",
    "We're at the tipping point - days start getting longer now.",
};
static const char* MSG_TINY_NIGHT_SHORTER[] = {
    "Tomorrow will be just a tiny bit shorter than today.",
    "Brighter days will return - but tomorrow loses a touch of sun.",
};

static const char* pickTemplate(time_t now, bool isDay, bool isLonger, bool isTiny) {
    const char** list;
    int n;
    if (isTiny) {
        if      (isDay && isLonger) { list = MSG_TINY_DAY_LONGER;    n = 3; }
        else if (isDay)             { list = MSG_TINY_DAY_SHORTER;   n = 3; }
        else if (isLonger)          { list = MSG_TINY_NIGHT_LONGER;  n = 2; }
        else                        { list = MSG_TINY_NIGHT_SHORTER; n = 2; }
    } else {
        if      (isDay && isLonger) { list = MSG_DAY_LONGER;         n = 5; }
        else if (isDay)             { list = MSG_DAY_SHORTER;        n = 3; }
        else if (isLonger)          { list = MSG_NIGHT_LONGER;       n = 3; }
        else                        { list = MSG_NIGHT_SHORTER;      n = 2; }
    }
    // Stable per-day selection — FNV-1a hash of the date string
    struct tm tinfo; localtime_r(&now, &tinfo);
    char key[16];
    snprintf(key, sizeof(key), "%04d%02d%02d", tinfo.tm_year + 1900, tinfo.tm_mon + 1, tinfo.tm_mday);
    uint32_t h = 2166136261u;
    for (char* p = key; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    return list[h % n];
}

// ── Mixed-weight word wrap ────────────────────────────────────────────────
// Walks the template (with **bold** markers), wraps at maxW, draws each
// segment with the right font. Returns the number of lines written.
static void renderWrappedMessage(const char* tmpl, const String& dur,
                                 int x, int y, int maxW, int lineHeight,
                                 uint16_t color, int maxLines) {
    String filled(tmpl);
    filled.replace("{d}", dur);

    // Tokenize at ** boundaries → segments (text, bold)
    struct Seg { String text; bool bold; };
    Seg segs[16];
    int nSegs = 0;
    bool boldNext = false;
    int from = 0;
    for (int i = 0; i + 1 <= (int)filled.length() && nSegs < 16; ) {
        int idx = filled.indexOf("**", i);
        if (idx < 0) {
            segs[nSegs++] = { filled.substring(from), boldNext };
            break;
        }
        segs[nSegs++] = { filled.substring(from, idx), boldNext };
        boldNext = !boldNext;
        from = idx + 2;
        i = from;
    }

    // Wrap into lines, drawing as we go
    gfx->setTextSize(1);
    gfx->setTextColor(color);
    int cursorX = x;
    int cursorY = y;
    int linesUsed = 1;

    for (int s = 0; s < nSegs; s++) {
        // Uniform 16px size with bold emphasis (matches crew-names readability)
        gfx->setFont(segs[s].bold ? FONT_BODY_BOLD : FONT_BODY);
        // Tokenize text on whitespace, keeping spaces with following words
        String text = segs[s].text;
        int len = text.length();
        int t0 = 0;
        while (t0 < len) {
            // Walk to next whitespace boundary (or end)
            int t1 = t0;
            bool isSpace = (text[t0] == ' ');
            if (isSpace) {
                while (t1 < len && text[t1] == ' ') t1++;
            } else {
                while (t1 < len && text[t1] != ' ') t1++;
            }
            String tok = text.substring(t0, t1);
            t0 = t1;

            int16_t bx, by; uint16_t tw, th;
            gfx->getTextBounds(tok.c_str(), 0, 0, &bx, &by, &tw, &th);

            // Drop leading spaces at the start of a new line
            if (isSpace && cursorX == x) continue;
            // Wrap if this token would overflow
            if (!isSpace && cursorX + (int)tw > x + maxW && cursorX > x) {
                cursorY += lineHeight;
                cursorX = x;
                if (++linesUsed > maxLines) return;
            }
            gfx->setCursor(cursorX, cursorY);
            gfx->print(tok);
            cursorX += (int)tw;
        }
    }
}

// ── Main render ───────────────────────────────────────────────────────────
void view_daylight_render() {
    time_t now;
    time(&now);

    // Guard: if NTP hasn't synced yet, time will be near epoch. Don't render
    // garbage — show a placeholder.
    if (now < 1735689600 /* Jan 1 2025 */) {
        gfx->fillScreen(COL_BG);
        gfx->setFont(FONT_SUBTITLE);
        gfx->setTextSize(1);
        display_drawCentered(DISP_H / 2, "Waiting for time sync...", COL_TEXT_DIM);
        return;
    }

    time_t yest = now - 86400;

    SunTimes tToday = suncalc_getTimes(now,  OSLO_LAT, OSLO_LNG);
    SunTimes tYest  = suncalc_getTimes(yest, OSLO_LAT, OSLO_LNG);
    SunPosition sp  = suncalc_getPosition(now, OSLO_LAT, OSLO_LNG);

    float altDeg    = (float)(sp.altitude * 180.0 / M_PI);
    bool  isMorning = now < tToday.solarNoon;
    Theme theme     = gradientColors(altDeg, isMorning);

    bool isDay = tToday.sunriseValid && tToday.sunsetValid &&
                 (now >= tToday.sunrise && now <= tToday.sunset);

    long todayLen = (long)(tToday.sunset - tToday.sunrise);
    long yestLen  = (long)(tYest.sunset  - tYest.sunrise);
    long delta    = todayLen - yestLen;
    bool isLonger = delta >= 0;
    bool isTiny   = (delta < 30 && delta > -30);

    // ── Diagnostic — print to Serial so we can verify suncalc on hardware ─
    Serial.printf("[DL] now=%ld sunrise=%ld sunset=%ld noon=%ld\n",
                  (long)now, (long)tToday.sunrise,
                  (long)tToday.sunset, (long)tToday.solarNoon);
    Serial.printf("[DL] altDeg=%.2f isDay=%d isMorning=%d isLonger=%d delta=%lds tiny=%d\n",
                  altDeg, isDay, isMorning, isLonger, (long)delta, isTiny);

    // Defensive guard: if suncalc returned bogus times where sunset≤sunrise,
    // mark them invalid so we don't try to position the sun against a zero
    // range. Forces the "before sunrise" branch which puts the sun at lineX1.
    if (tToday.sunsetValid && tToday.sunset <= tToday.sunrise + 60) {
        Serial.println("[DL] WARN sunset<=sunrise+60s — flagging invalid");
        tToday.sunsetValid = false;
        tToday.sunriseValid = false;
    }

    gfx->fillScreen(theme.bg);

    // ── Horizon + sun ─────────────────────────────────────────────────────
    bool legendsOn = state.legendsOn;
    int  horizonY  = legendsOn ? 110 : 185;
    int  lineX1    = 30;
    int  lineX2    = DISP_W - 30;

    // Horizon line — 2px with legends on, 3px in graphics-only mode
    if (legendsOn) {
        gfx->fillRect(lineX1, horizonY, (lineX2 - lineX1), 2, theme.text);
    } else {
        gfx->fillRect(lineX1, horizonY - 1, (lineX2 - lineX1), 3, theme.text);
    }

    // Sun arc peak height — driven by DAY LENGTH so winter is flat, summer soars
    float daylightHrs = todayLen / 3600.0f;             // 0..24
    float peakNorm    = daylightHrs / 16.0f;            // 16h ≈ full peak
    if (peakNorm > 1.0f) peakNorm = 1.0f;
    float peakMax     = legendsOn ? 90.0f : 160.0f;
    float peakH       = peakNorm * peakMax;
    if (peakH < 12.0f) peakH = 12.0f;

    int sx, sy;
    if (isDay) {
        float frac = (float)(now - tToday.sunrise) /
                     (float)(tToday.sunset - tToday.sunrise);
        sx = (int)(lineX1 + (lineX2 - lineX1) * frac);
        sy = (int)(horizonY - sinf((float)M_PI * frac) * peakH);
    } else if (now < tToday.sunrise) {
        sx = lineX1;
        sy = horizonY + 8;
    } else {
        sx = lineX2;
        sy = horizonY + 8;
    }

    // Slightly bigger sun dot for visibility
    int sunR = legendsOn ? 5 : 7;
    gfx->fillCircle(sx, sy, sunR, theme.text);

    // Current time floating above the sun — always small (Helvetica TINY)
    char nowStr[8];
    {
        struct tm tinfo; localtime_r(&now, &tinfo);
        snprintf(nowStr, sizeof(nowStr), "%02d:%02d", tinfo.tm_hour, tinfo.tm_min);
    }
    gfx->setFont(FONT_TINY);
    gfx->setTextSize(1);
    gfx->setTextColor(theme.text);
    int16_t bx, by; uint16_t tw, th;
    gfx->getTextBounds(nowStr, 0, 0, &bx, &by, &tw, &th);
    int nowX = sx - (int)tw / 2;
    if (nowX < lineX1) nowX = lineX1;
    if (nowX + (int)tw > lineX2) nowX = lineX2 - (int)tw;
    gfx->setCursor(nowX, sy - sunR - 4);
    gfx->print(nowStr);

    // Sunrise / sunset — Inter Body 16 (matches the rest of the daylight
    // text). Pushed down a bit more when legends are off.
    gfx->setFont(FONT_BODY);
    int timeY = legendsOn ? horizonY + 22 : horizonY + 28;
    if (tToday.sunriseValid) {
        String sr = fmtHM(tToday.sunrise);
        gfx->setCursor(lineX1, timeY);
        gfx->print(sr);
    }
    if (tToday.sunsetValid) {
        String ss = fmtHM(tToday.sunset);
        gfx->getTextBounds(ss.c_str(), 0, 0, &bx, &by, &tw, &th);
        gfx->setCursor(lineX2 - (int)tw, timeY);
        gfx->print(ss);
    }

    if (!legendsOn) return; // text-off mode

    // ── Big multi-line message + Oslo label ───────────────────────────────
    String dur = fmtDeltaWords(delta, isLonger);
    const char* tmpl = pickTemplate(now, isDay, isLonger, isTiny);

    Serial.printf("[DL] tmpl: %s  dur: %s\n", tmpl, dur.c_str());

    // Uniform 16px size, line height 22, max 3 lines
    renderWrappedMessage(tmpl, dur, lineX1, 175, DISP_W - 60, 22,
                         theme.text, 3);

    // City label at bottom
    gfx->setFont(FONT_TINY);
    gfx->setTextColor(theme.text);
    gfx->setCursor(lineX1, DISP_H - 6);
    gfx->print("Oslo, Norway");
}
