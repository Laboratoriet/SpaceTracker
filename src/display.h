#pragma once

#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include "config.h"
#include "fonts.h"

// Global display objects. `gfx` is now an Arduino_Canvas backed by PSRAM —
// views render to the canvas, then `display_flush()` pushes the full frame
// to the AMOLED in one shot, eliminating the visible mid-redraw flicker.
extern Arduino_DataBus *bus;
extern Arduino_GFX     *gfx;

void display_init();
void display_flush();
void display_setBrightness(uint8_t brightness);

// Draw text centered horizontally at given Y (baseline for u8g2 fonts)
// Uses getTextBounds for accurate centering with any font
void display_drawCentered(int16_t y, const char* str, uint16_t color);
void display_drawCenteredString(int16_t y, const String& str, uint16_t color);
