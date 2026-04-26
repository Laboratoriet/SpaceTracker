#include "view_names.h"
#include "display.h"
#include "config.h"

void view_names_render(const CrewInfo &crew) {
    gfx->fillScreen(COL_BG);

    if (!crew.valid) {
        gfx->setFont(FONT_SUBTITLE);
        gfx->setTextSize(1);
        display_drawCentered(130, "No data yet", COL_TEXT_DIM);
        return;
    }

    // Group by craft
    String crafts[5];
    int craftCount = 0;
    for (int i = 0; i < crew.count && i < 20; i++) {
        bool found = false;
        for (int c = 0; c < craftCount; c++) {
            if (crafts[c] == crew.crafts[i]) { found = true; break; }
        }
        if (!found && craftCount < 5) {
            crafts[craftCount++] = crew.crafts[i];
        }
    }

    // Layout: columns by craft — no top header, use full height
    int colWidth = DISP_W / max(craftCount, 1);
    int startY = 24;

    for (int c = 0; c < craftCount; c++) {
        int cx = 10 + c * colWidth;
        int cy = startY;
        int maxTextW = colWidth - 20;  // padding on each side

        // Craft header
        uint16_t headerColor = (c == 0) ? COL_ISS_DOT : COL_CSS_DOT;
        gfx->setFont(FONT_HEADER);
        gfx->setTextSize(1);
        gfx->setTextColor(headerColor);
        gfx->setCursor(cx, cy);
        gfx->print(crafts[c]);
        cy += 28;

        // Crew names
        gfx->setFont(FONT_BODY);
        gfx->setTextColor(COL_TEXT_BRIGHT);
        int16_t x1, y1;
        uint16_t tw, th;
        for (int i = 0; i < crew.count && i < 20; i++) {
            if (crew.crafts[i] == crafts[c]) {
                if (cy > DISP_H - 8) break;

                String name = crew.names[i];

                // Truncate using actual text measurement
                gfx->getTextBounds(name.c_str(), 0, 0, &x1, &y1, &tw, &th);
                while (tw > (uint16_t)maxTextW && name.length() > 3) {
                    name = name.substring(0, name.length() - 1);
                    gfx->getTextBounds(name.c_str(), 0, 0, &x1, &y1, &tw, &th);
                }

                gfx->setCursor(cx, cy);
                gfx->print(name);
                cy += 22;
            }
        }
    }
}
