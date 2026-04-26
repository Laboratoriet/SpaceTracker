// Splash / boot screen — minimal title + status line, shown while WiFi+NTP
// connect and the first data fetch completes.

#include "view_splash.h"
#include "display.h"
#include "config.h"
#include "fonts.h"

#include <Arduino.h>

void view_splash_render(const char* status) {
    gfx->fillScreen(COL_BG);

    // Title — "SPACE TRACKER" on a single line, Inter Bold 24
    gfx->setFont(FONT_TITLE);
    gfx->setTextSize(1);
    gfx->setTextColor(COL_TEXT_BRIGHT);

    const char* title = "SPACE TRACKER";
    int16_t bx, by; uint16_t tw, th;
    gfx->getTextBounds(title, 0, 0, &bx, &by, &tw, &th);
    gfx->setCursor((DISP_W - (int)tw) / 2, 120);
    gfx->print(title);

    if (status && *status) {
        gfx->setFont(FONT_SUBTITLE);
        gfx->setTextColor(COL_TEXT_DIM);
        display_drawCentered(160, status, COL_TEXT_DIM);
    }
}
