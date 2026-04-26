#include "display.h"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);

// Real AMOLED output device
static Arduino_RM67162 *amoled = new Arduino_RM67162(bus, PIN_LCD_RST, 0);
// Canvas (PSRAM-backed framebuffer). Views render here; flush() pushes whole
// 536×240 RGB565 frame to the AMOLED in one go — no visible refresh blink.
static Arduino_Canvas *canvas = nullptr;
Arduino_GFX *gfx = nullptr;

void display_init() {
    // GPIO 38 is the green status LED on the non-touch AMOLED board (NOT
    // display power). Drive it HIGH at boot so the LED lights immediately;
    // main.cpp will later override based on the persisted state.ledOn value.
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
    delay(50);

    if (!amoled->begin()) {
        Serial.println("ERROR: amoled->begin() failed!");
        return;
    }
    amoled->setRotation(1);              // landscape 536×240
    amoled->fillScreen(COL_BG);

    // Allocate canvas (~257 KB in PSRAM)
    canvas = new Arduino_Canvas(DISP_W, DISP_H, amoled);
    if (!canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
        Serial.println("ERROR: canvas->begin() failed!");
        return;
    }
    gfx = canvas;
    Serial.println("Display + canvas initialized OK");

    gfx->fillScreen(COL_BG);
    display_setBrightness(DEFAULT_BRIGHTNESS);
}

void display_flush() {
    if (canvas) canvas->flush();
}

void display_setBrightness(uint8_t brightness) {
    bus->beginWrite();
    bus->writeC8D8(0x51, brightness);
    bus->endWrite();
}

void display_drawCentered(int16_t y, const char* str, uint16_t color) {
    gfx->setTextColor(color);

    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(str, 0, y, &x1, &y1, &w, &h);

    gfx->setCursor((DISP_W - w) / 2, y);
    gfx->print(str);
}

void display_drawCenteredString(int16_t y, const String& str, uint16_t color) {
    display_drawCentered(y, str.c_str(), color);
}
