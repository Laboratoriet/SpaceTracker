#pragma once

// LilyGo T-Display S3 AMOLED pin configuration
// Display: RM67162, 240x536, QSPI interface

// QSPI display pins
#define PIN_LCD_CS   6
#define PIN_LCD_SCK  47
#define PIN_LCD_D0   18
#define PIN_LCD_D1   7
#define PIN_LCD_D2   48
#define PIN_LCD_D3   5
#define PIN_LCD_RST  17

// Green status LED (independently controllable on the non-touch AMOLED variant).
// Confirmed via LilyGo factory firmware:
//   T-Display-S3-AMOLED/examples/factory/pins_config.h → `#define PIN_LED 38`
//   factory.ino comment: "Non-touch version, IO38 is an onboard LED light"
// Display has its own power rail and does NOT depend on this pin.
#define PIN_LED 38
// Backwards-compat alias (older code referenced this as PIN_POWER_ON)
#define PIN_POWER_ON PIN_LED

// Buttons
#define PIN_BUTTON_1 0
#define PIN_BUTTON_2 21

// Battery
#define PIN_BAT_VOLT 4

// I2C (touch)
#define PIN_IIC_SCL  40
#define PIN_IIC_SDA  39
#define PIN_TOUCH_INT 16
#define PIN_TOUCH_RES 13
