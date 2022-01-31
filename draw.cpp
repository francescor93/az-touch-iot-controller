#include "draw.h"

// Include the appropriate library
#ifdef ESP32
  #include <TFT_eSPI.h>
#endif
#ifdef ESP8266
  #include <MiniGrafx.h>
  #include <ILI9341_SPI.h>
#endif
#include "DejaVu_Sans_10.h"
#include "DejaVu_Sans_Bold_12.h"

// Define the appropriate pins
#ifdef ESP8266
  #define TFT_DC D2
  #define TFT_CS D1
#endif

// Initialize the appropriate library
#ifdef ESP32
  TFT_eSPI gfx = TFT_eSPI();
#endif
#ifdef ESP8266
  ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC);
  MiniGrafx gfx = MiniGrafx(&tft, 2, palette);
#endif

// Configure screen colors
#ifdef ESP32
  uint16_t palette[] = {
    0x0000,   // 0: Black
    0xFFFF,   // 1: White
    0x001F,   // 2: Blue
    0xFFE0,   // 3: Yellow
    0x07E0,   // 4: Green
    0xF800,   // 5: Red
    0xFE19,   // 6: Pink
    0x07FF    // 7: Cyan
  };
#endif
#ifdef ESP8266
  uint16_t palette[] = {
    0x0000,   // 0: Black
    0xFFFF,   // 1: White
    0x001F,   // 2: Blue
    0xFFE0   // 3: Yellow
  };
#endif

// Define display helpers
void displayInit() {
  gfx.init();
}
void displaySetRotation(int pos) {
  gfx.setRotation(pos);
}
void displayFill(uint16_t color) {
  #ifdef ESP32
    gfx.fillScreen(palette[color]);
  #endif
  #ifdef ESP8266
    gfx.fillBuffer(color);
  #endif
}
void displayCommit() {
  #ifdef ESP8266
    gfx.commit();
  #endif
}
void displaySetColor(uint16_t color) {
  #ifdef ESP32
    gfx.setTextColor(palette[color]);
  #endif
  #ifdef ESP8266
    gfx.setColor(color);
  #endif
}
void displayFontTitle() {
  #ifdef ESP32
    gfx.setFreeFont(&DejaVu_Sans_Bold_12);
  #endif
  #ifdef ESP8266
    //gfx.setFont(ArialRoundedMTBold_14);
    gfx.setFont(DejaVu_Sans_Bold_12);
  #endif
}
void displayFontText() {
  #ifdef ESP32
    gfx.setFreeFont(&DejaVu_Sans_10);
  #endif
  #ifdef ESP8266
    //gfx.setFont(ArialMT_Plain_10);
    gfx.setFont(DejaVu_Sans_10);
  #endif
}
void displayAlignCenter() {
  #ifdef ESP32
    gfx.setTextDatum(TC_DATUM);
  #endif
  #ifdef ESP8266
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  #endif
}
void displayAlignCenterMiddle() {
  #ifdef ESP32
    gfx.setTextDatum(MC_DATUM);
  #endif
  #ifdef ESP8266
    gfx.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  #endif
}
void displayAlignLeft() {
  #ifdef ESP32
    gfx.setTextDatum(TL_DATUM);
  #endif
  #ifdef ESP8266
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  #endif
}
void displayWrite(String text, int x, int y) {
  #ifdef ESP32
    gfx.drawString(text, x, y);
  #endif
  #ifdef ESP8266
    gfx.drawString(x, y, text);
  #endif
}
void displayDrawPixel(int x, int y, uint16_t color) {
  #ifdef ESP32
    gfx.drawPixel(x, y, palette[color]);
  #endif
  #ifdef ESP8266
    gfx.setColor(color);
    gfx.setPixel(x, y);
  #endif
}
void displayFillCircle(int16_t x, int16_t y, int radius, uint16_t color) {
  #ifdef ESP32
    gfx.fillCircle(x, y, radius, palette[color]);
  #endif
  #ifdef ESP8266
    gfx.setColor(color);
    gfx.fillCircle(x, y, radius);
  #endif
}
void displayDrawBitmap(uint8_t *content, int x, int y, int width, int height, uint16_t color) {
  #ifdef ESP32
    gfx.drawXBitmap(x, y, content, width, height, palette[color]);
  #endif
  #ifdef ESP8266
    gfx.setColor(color);
    gfx.drawXbm(x, y, width, height, (char *)content);
  #endif
}
void displayDrawRect(int x, int y, int width, int height, uint16_t color) {
  #ifdef ESP32
    gfx.drawRect(x, y, width, height, palette[color]);
  #endif
  #ifdef ESP8266
    gfx.setColor(color);
    gfx.drawRect(x, y, width, height);
  #endif
}
void displayFillRect(int x, int y, int width, int height, uint16_t color) {
  #ifdef ESP32
    gfx.fillRect(x, y, width, height, palette[color]);
  #endif
  #ifdef ESP8266
    gfx.setColor(color);
    gfx.fillRect(x, y, width, height);
  #endif
}
void displayDrawHLine(int x, int y, int width, uint16_t color) {
  #ifdef ESP32
    gfx.drawFastHLine(x, y, width, palette[color]);
  #endif
  #ifdef ESP8266
    gfx.setColor(color);
    gfx.drawHorizontalLine(x, y, width);
  #endif
}
void displayDrawVLine(int x, int y, int height, uint16_t color) {
  #ifdef ESP32
    gfx.drawFastVLine(x, y, height, palette[color]);
  #endif
  #ifdef ESP8266
    gfx.setColor(color);
    gfx.drawVerticalLine(x, y, height);
  #endif
}
