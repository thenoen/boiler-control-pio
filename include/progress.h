#include <Arduino.h>
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

void toggleProgressIndicator(Adafruit_ST7735 tft);
void drawProgressIndicator(Adafruit_ST7735 tft);
void drawSegment(Adafruit_ST7735 tft, int x, int b, uint16_t color);