#include "view_crew.h"
#include "display.h"
#include "config.h"
#include "state.h"

void view_crew_render(const CrewInfo &crew, bool wifiConnected) {
    gfx->fillScreen(COL_BG);

    if (!crew.valid) {
        gfx->setFont(FONT_SUBTITLE);
        gfx->setTextSize(1);
        display_drawCentered(130, "Fetching data...", COL_TEXT_DIM);
        return;
    }

    // ── Big number ──
    char numStr[8];
    snprintf(numStr, sizeof(numStr), "%d", crew.count);

    gfx->setFont(FONT_BIG_NUM);
    gfx->setTextSize(1);

    // Measure number for centering
    int16_t x1, y1;
    uint16_t tw, th;
    gfx->getTextBounds(numStr, 0, 0, &x1, &y1, &tw, &th);

    // Center the number + subtitle block vertically
    // Number ~92px + gap ~30px + subtitle ~16px = ~138px total
    int blockH = th + 30 + 16;
    int startY = (DISP_H - blockH) / 2 + th;  // baseline of number

    gfx->setTextColor(COL_TEXT_BRIGHT);
    gfx->setCursor((DISP_W - tw) / 2, startY);
    gfx->print(numStr);

    // ── Subtitle — toggleable via legends, gap below number ──
    if (state.legendsOn) {
        gfx->setFont(FONT_SUBTITLE);
        display_drawCentered(startY + 40, "humans in space right now", COL_TEXT_DIM);
    }

    // WiFi indicator
    if (!wifiConnected) {
        gfx->fillCircle(DISP_W - 8, 6, 3, COL_WIFI_ERR);
    }

    // Stale data indicator
    if (crew.valid && (millis() - crew.lastFetch > ASTROS_REFRESH_MS * 3)) {
        gfx->fillCircle(DISP_W - 18, 6, 3, COL_STALE);
    }
}
