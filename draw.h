#ifndef DRAWER
  #define DRAWER

  // Include the appropriate library
  #ifdef ESP32
    #include <TFT_eSPI.h>
  #endif
  #ifdef ESP8266
    #include <MiniGrafx.h>
    #include <ILI9341_SPI.h>
  #endif

  // Define the appropriate pins
  #ifdef ESP8266
    #define TFT_DC D2
    #define TFT_CS D1
  #endif

  // Initialize the appropriate library
  #ifdef ESP32
    extern TFT_eSPI gfx;
  #endif
  #ifdef ESP8266
    extern ILI9341_SPI tft;
    extern MiniGrafx gfx;
  #endif

  // Configure screen colors
  extern uint16_t palette[];

  // Define display helpers
  extern void displayInit();
  extern void displaySetRotation(int pos);
  extern void displayFill(uint16_t color);
  extern void displayCommit();
  extern void displaySetColor(uint16_t color);
  extern void displayFontTitle();
  extern void displayFontText();
  extern void displayAlignCenter();
  extern void displayAlignCenterMiddle();
  extern void displayAlignLeft();
  extern void displayWrite(String text, int x, int y);
  extern void displayFillCircle(int16_t x, int16_t y, int radius, uint16_t color);
  extern void displayDrawBitmap(uint8_t *content, int x, int y, int width, int height, uint16_t color);
  extern void displayDrawRect(int x, int y, int width, int height, uint16_t color);
  extern void displayFillRect(int x, int y, int width, int height, uint16_t color);
  extern void displayDrawHLine(int x, int y, int width, uint16_t color);
  extern void displayDrawVLine(int x, int y, int height, uint16_t color);

#endif
