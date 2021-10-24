#include "progress.h"


int a = 8, b = 30, d = 10;
int y = 120;
int e = 5;
int s = b + d + e;
int x = -s;
int step = 10;
int angle = 6;
boolean progressToggle = true;

void toggleProgressIndicator(Adafruit_ST7735 tft) {
	int x = 150;
	int y = 8;
	int r = 4;
	if (progressToggle) {
		tft.fillCircle(x, y, r, ST77XX_YELLOW);
	} else {
		tft.fillCircle(x, y, r, ST77XX_BLACK);
	}
	progressToggle = !progressToggle;
}

void drawProgressIndicator(Adafruit_ST7735 tft) {

	x += 5;

	for (int i = 0; i < 5; i++) {
		drawSegment(tft, x + s * i - d, d, ST77XX_BLACK);
		drawSegment(tft, x + s * i, 30, ST77XX_GREEN);
	}
	if (x > 0) {
		x = -s;
	}
}

void drawSegment(Adafruit_ST7735 tft, int x, int b, uint16_t color) {
	tft.fillTriangle(x, y, x + b - angle, y + a, x - angle, y + a, color);
	tft.fillTriangle(x + b, y, x + b - angle, y + a, x, y, color);
}