#include <Arduino.h>

#include <Wire.h>
#include "RTClib.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <Adafruit_GFX.h>	 // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>
// #include "custom-font.h" //not used
#include "custom-font-7p.h"

#include "free_memory.h"
#include "progress.h"
#include "history.h"

/*
 * Used Arduino pins
 * 
 * 		DIGITAL (the same as printed on the board)
 * 		0	-
 * 		1	-
 * 		2	i2c bus - thermometers // !!! conflict with interrupts
 * 		3	-
 * 		4 	! used in code but not wired
 * 		5	boiler control - scheduling of heating
 * 		6	boiler control - switching of water output
 * 		7	boiler control - switching of water output
 * 		8	TFT display
 * 		9	TFT display
 * 		10	TFT display
 * 		11	TFT display
 * 		12	-
 * 		13	TFT display
 * 
 *  Pins supportig hardware interrupts: 2, 3
 * 
 */

#define TFT_CS 10
#define TFT_RST 8 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC 9

#define COLOR_GRAY 0x83EF

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

RTC_DS3231 rtc;
// char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
int DATE_TIME_X = 1;
int DATE_TIME_Y = 12;

int T1_POSITION_X = 1;
int T1_POSITION_Y = 28;
int T2_POSITION_X = 1;
int T2_POSITION_Y = T1_POSITION_Y + 15;
int T3_POSITION_X = 1;
int T3_POSITION_Y = T2_POSITION_Y + 15;

int BOILER1_I2C_INDEX = 1;
int BOILER2_I2C_INDEX = 0;
uint8_t *BOILER1_I2C_ADDRESS = new uint8_t[8];
uint8_t *BOILER2_I2C_ADDRESS = new uint8_t[8];
int BOILER1_PIN = 6;
int BOILER2_PIN = 7;
int B2_HEATING_PIN = 5;
int EXTRA_PIN = 4;

int PANEL_I2C_INDEX = 2;

int activeBoiler = -1;
int boilerToActivate = -1;
int activationInProgress = 0;
uint32_t boilerActivationTime;
int VALVE_SWITCH_TEMPERATURE = 1; // temperature difference between boilers for valve switching
int VALVE_SWITCH_DURATION = 60;	  // duration of impulse for switching of valve in seconds

#define ONE_WIRE_BUS 2
// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);
// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);
int deviceCount = 0;

char previousText[2][12];
int PREV_TIME = 1;
int PREV_DATE = 2;

char previousTemperature[5][3][30]; // todo: rename to 'previousText'

// ============== WATER LEVEL MEASUREMENT =====================================

int A_IN_PIN = 14;		  // A0
float DA_RATIO = 0.00488; // 5/1024
int WATER_DENSITY = 1000;
float GRAVITATIONAL_ACCELERATION = 9.81;

const int mCount = 10;
int currentMeasurement = 0;
float measuredWaterLevels[mCount];

// ============================================================================

//####################### METHOD FORWARD DECLARATION #############################
void drawDateTime(DateTime now);
void printTimeToSerial();
void testdrawtext(char *text, uint16_t color);
void detectTempSensors();
void drawTemperature(char *text, float temperature, int positionX, int positionY, uint16_t color);
void drawTemperature2(int i2c_index, float temperature, int positionX, int positionY, uint16_t color);
void writeWaterLevelText(int i2c_index, int digitalInput, int positionX, int positionY, uint16_t color);
void evaluateValve(float temp1, float temp2);
void activateBoiler();
const char *indexToDay(uint8_t index);
uint16_t getColor(int boiler);
void evaluateHeating();
boolean isErrorTemperature(float temperature);
int scmp(const char *X, const char *Y);
void calculateWaterLevel(DateTime now, Adafruit_ST7735 tft);

void setup(void)
{
	sensors.begin(); // Start up the library
	sensors.getAddress(BOILER1_I2C_ADDRESS, BOILER1_I2C_INDEX);
	sensors.getAddress(BOILER2_I2C_ADDRESS, BOILER2_I2C_INDEX);

	sensors.setResolution(12);
	sensors.setWaitForConversion(true);

	Serial.begin(115200);

	tft.initR(INITR_BLACKTAB);
	tft.setTextWrap(true);
	tft.setFont(&LiberationMono_Regular7pt7b);

	// Serial.println(F("Initialized"));

	tft.setRotation(1);
	tft.fillScreen(ST77XX_BLACK);
	tft.drawFastHLine(0, 15, 160, COLOR_GRAY);

	int b = rtc.begin();

	if (!b)
	{
		// Serial.println("RTC problem");
		while (1)
			;
	}
	else
	{
		// Serial.println("RTC ok");
	}

	if (rtc.lostPower())
	{
		// Serial.println("RTC lost power, lets set the time!");
		// Following line sets the RTC to the date & time this sketch was compiled
		rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
		// Following line sets the RTC with an explicit date & time
		// for example to set January 27 2017 at 12:56 you would call:
		// rtc.adjust(DateTime(2017, 1, 27, 12, 56, 0));
	}
		// rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

	pinMode(BOILER1_PIN, OUTPUT);
	digitalWrite(BOILER1_PIN, LOW);
	pinMode(BOILER2_PIN, OUTPUT);
	digitalWrite(BOILER2_PIN, LOW);
	pinMode(B2_HEATING_PIN, OUTPUT);
	pinMode(EXTRA_PIN, OUTPUT);
	//	activateBoiler(); // should not be needed here

		// detectTempSensors();
	//	strcpy(textHistory['time'], "empty");
	//	strcpy(textHistory['date'], "empty");
	strcpy(previousTemperature[BOILER1_I2C_INDEX][0], "B1");
	strcpy(previousTemperature[BOILER2_I2C_INDEX][0], "B2");
	strcpy(previousTemperature[PANEL_I2C_INDEX][0], "T3");
}

void loop()
{
	// long start = micros();
	delay(500);

	sensors.requestTemperatures();

	float boiler1Temperature = sensors.getTempCByIndex(BOILER1_I2C_INDEX);
		// boiler1Temperature = sensors.getTempC(BOILER1_I2C_ADDRESS);
	float boiler2Temperature = sensors.getTempCByIndex(BOILER2_I2C_INDEX);
		// boiler2Temperature = sensors.getTempC(BOILER2_I2C_ADDRESS);
	float panelTemperature = sensors.getTempCByIndex(PANEL_I2C_INDEX);

	//	Serial.print("temperature: ");
	//	Serial.println(boiler2Temperature);

	uint16_t color;
	color = getColor(BOILER1_PIN);
	drawTemperature2(BOILER1_I2C_INDEX, boiler1Temperature, T1_POSITION_X, T1_POSITION_Y, color);
	color = getColor(BOILER2_PIN);
	drawTemperature2(BOILER2_I2C_INDEX, boiler2Temperature, T2_POSITION_X, T2_POSITION_Y, color);
	drawTemperature2(3, abs(boiler1Temperature - boiler2Temperature), 90, ((T1_POSITION_Y + T2_POSITION_Y) / 2), ST77XX_WHITE);

	drawTemperature2(PANEL_I2C_INDEX, panelTemperature, T3_POSITION_X, T3_POSITION_Y, ST77XX_WHITE);

	evaluateValve(boiler1Temperature, boiler2Temperature);
	activateBoiler();
	evaluateHeating();
	DateTime now = rtc.now();
	drawDateTime(now);
	calculateWaterLevel(now, tft);

	// printTimeToSerial();

	// output pin - not yet determined what it will be used for
	//	uint8_t second = rtc.now().second();
	//	if ((second % 10) == 0) {
	//		digitalWrite(EXTRA_PIN, HIGH);
	//	} else {
	//		digitalWrite(EXTRA_PIN, LOW);
	//	}

	toggleProgressIndicator(tft);

	// Serial.print("mem = ");
	// Serial.println(freeMemory());

	// drawProgressIndicator(tft);

	// long end = micros();
	// long duration = end - start;
	//	Serial.print("loop duration: ");
	//	Serial.println(duration);
}

boolean isErrorTemperature(float temperature)
{
	return temperature == -127.0;
}

uint16_t getColor(int boiler)
{
	if (activeBoiler == boiler)
	{
		return ST77XX_RED;
	}
	else
	{
		//		if ((random(0, 10) % 2) == 0) {
		//			return ST7735_BLUE;
		//		} else {
		//			return ST7735_CYAN;
		//		}
		return ST77XX_WHITE;
	}
}

void testdrawtext(char *text, uint16_t color)
{
	tft.setTextColor(color);
	tft.println(text);
}

void printTimeToSerial()
{
	DateTime now = rtc.now();

	Serial.println("Current Date & Time: ");
	Serial.print(now.year(), DEC);
	Serial.print('/');
	Serial.print(now.month(), DEC);
	Serial.print('/');
	Serial.print(now.day(), DEC);
	Serial.print(" (");
	// Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
	Serial.print(") ");
	Serial.print(now.hour(), DEC);
	Serial.print(':');
	Serial.print(now.minute(), DEC);
	Serial.print(':');
	Serial.print(now.second(), DEC);
	Serial.println();

	Serial.println("Unix Time: ");
	Serial.print("elapsed ");
	Serial.print(now.unixtime());
	Serial.print(" seconds/");
	Serial.print(now.unixtime() / 86400L);
	Serial.println(" days since 1/1/1970");

	Serial.println("----------------------------------");
}

void drawDateTime(DateTime now)
{
	char resultText[11];
	int16_t x = 0, y = 0;
	uint16_t w = 0, h = 0;

	const char *dayName = indexToDay(now.dayOfTheWeek());

	// tft.setCursor(DATE_TIME_X, DATE_TIME_Y);
	tft.setTextColor(ST77XX_WHITE);

	//----------------------------
	//	sprintf(result, "%s %02d:%02d:%02d\n%02d.%02d.%02d", dayName, now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
	//	tft.getTextBounds(result, DATE_TIME_X, DATE_TIME_Y, &x, &y, &w, &h);
	//	w += 2; // a bug, rectangle of text may not be calculated correctly
	//	tft.fillRect(x - 1, y - 1, w, h, ST77XX_BLACK); // "-1" is for fixing of calculation of text rectangle
	//
	//	tft.setCursor(DATE_TIME_X, DATE_TIME_Y);
	//	tft.setTextColor(ST77XX_WHITE);
	//	tft.print(result);
	//----------------------------

	sprintf(resultText, "%s %02d.%02d.", dayName, now.day(), now.month());
	if (strcmp(previousText[PREV_TIME], resultText) != 0)
	{
		tft.setCursor(DATE_TIME_X, DATE_TIME_Y);
		strcpy(previousText[PREV_TIME], resultText);
		tft.getTextBounds(resultText, DATE_TIME_X, DATE_TIME_Y, &x, &y, &w, &h);
		w += 2;												// a bug, rectangle of text may not be calculated correctly
		tft.fillRect(x - 1, y - 1, w, h + 1, ST77XX_BLACK); // "-1" is for fixing of calculation of text rectangle
		tft.println(resultText);
	}
	else
	{
		tft.getTextBounds(previousText[PREV_TIME], DATE_TIME_X, DATE_TIME_Y, &x, &y, &w, &h);
		w += 2; // a bug, rectangle of text may not be calculated correctly
	}

	sprintf(resultText, " / %02d:%02d", now.hour(), now.minute());
	// sprintf(resultText, " - %02d.%02d.", now.second(), now.second());
	if (strcmp(previousText[PREV_DATE], resultText) != 0)
	{
		tft.setCursor(DATE_TIME_X + w, DATE_TIME_Y);
		strcpy(previousText[PREV_DATE], resultText);
		tft.getTextBounds(resultText, DATE_TIME_X + w, DATE_TIME_Y, &x, &y, &w, &h); // todo: why +w ???
		tft.fillRect(x - 1, y, w + 1, h, ST77XX_BLACK);								 // "-1" is for fixing of calculation of text rectangle
		tft.print(resultText);
	}
}

void detectTempSensors()
{
	Serial.print("Locating devices...");
	Serial.print("Found ");
	deviceCount = sensors.getDeviceCount();
	Serial.print(deviceCount, DEC);
	Serial.println(" devices.");
	Serial.println("");

	tft.setTextWrap(true);
	tft.setCursor(0, 15);

	char result[30];
	sprintf(result, "count: %d", deviceCount);
	testdrawtext(result, ST7735_BLUE);
	Serial.println(result);

	sensors.getAddress(BOILER1_I2C_ADDRESS, BOILER1_I2C_INDEX);
	sprintf(result, "b1: %d", BOILER1_I2C_ADDRESS);
	testdrawtext(result, ST7735_BLUE);
	Serial.println(result);

	sensors.getAddress(BOILER2_I2C_ADDRESS, BOILER2_I2C_INDEX);
	sprintf(result, "b2: %d", BOILER2_I2C_ADDRESS);
	testdrawtext(result, ST7735_BLUE);
	Serial.println(result);
	delay(5000);
}

void drawTemperature(char *text, float temperature, int positionX, int positionY, uint16_t color)
{

	int16_t x, y;
	uint16_t w, h;
	char result[50];
	char tmp[10];

	if (isErrorTemperature(temperature))
	{
		color = ST77XX_ORANGE;
		strcpy(tmp, " ?.?");
	}
	else
	{
		dtostrf(temperature, 4, 1, tmp);
	}

	sprintf(result, "%s %s C", text, tmp);
	tft.setCursor(positionX, positionY);
	tft.getTextBounds(result, positionX, positionY, &x, &y, &w, &h);
	tft.fillRect(x, y, w, h, ST77XX_BLACK);
	tft.setTextColor(color);
	tft.print(result);
}

void drawTemperature2(int i2c_index, float temperature, int positionX, int positionY, uint16_t color)
{

	int16_t x, y;
	uint16_t w, h;
	char textToPrint[30];
	char temperatureAsString[10];
	char cmpColor[3];

	if (isErrorTemperature(temperature))
	{
		color = ST77XX_ORANGE;
		//		strcpy(tmp, " ?.?");
		strcpy(textToPrint, previousTemperature[i2c_index][1]);
	}
	else
	{
		dtostrf(temperature, 4, 1, temperatureAsString);
		sprintf(textToPrint, "%s %s C", previousTemperature[i2c_index][0], temperatureAsString);
	}

	dtostrf(color, 2, 0, cmpColor);

	int cmpTemp = scmp(previousTemperature[i2c_index][1], textToPrint);
	int cmpCol = scmp(previousTemperature[i2c_index][2], cmpColor);

	if (cmpTemp != 0 || cmpCol != 0)
	{

		//		Serial.println("diff: ");
		//		Serial.print(previousTemperture[i2c_index][1]);
		//		Serial.print(" = ");
		//		Serial.print(result);
		//		Serial.print(" -> ");
		//		Serial.println(cmpTemp);

		tft.getTextBounds(previousTemperature[i2c_index][1], positionX, positionY, &x, &y, &w, &h);
		strcpy(previousTemperature[i2c_index][1], textToPrint);
		dtostrf(color, 2, 0, previousTemperature[i2c_index][2]);
		tft.setCursor(positionX, positionY);
		tft.fillRect(x, y, w, h, ST77XX_BLACK);
		tft.setTextColor(color);
		//		delay(100);
		tft.print(textToPrint);
	}
}

int scmp(const char *s1, const char *s2)
{
	//	Serial.println("------------------------");
	//	Serial.println("diff: ");
	//	Serial.print(s1);
	//	Serial.print(" = ");
	//	Serial.println(s2);

	while (*s1 && (*s1 == *s2))
	{

		//		Serial.print(*s1);
		//		Serial.print(" = ");
		//		Serial.println(*s2);

		s1++;
		s2++;
	}

	//	Serial.print(*s1);
	//	Serial.print(" = ");
	//	Serial.println(*s2);
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void printTemperatureSensorsToSerial()
{
	float temperature;
	for (int i = 0; i < deviceCount; i++)
	{
		Serial.print("Sensor ");
		Serial.print(i + 1);
		Serial.print(" : ");
		temperature = sensors.getTempCByIndex(i);
		Serial.print(temperature);
		Serial.print((char)176); // shows degrees character
		Serial.println("C");
	}
}

void evaluateValve(float temp1, float temp2)
{

	if (isErrorTemperature(temp1) || isErrorTemperature(temp2))
	{
		// Serial.println("error measurement, skipping boiler evaluation");
		return;
	}

	if (activationInProgress)
	{
		// Serial.println("boiler activation in progress, skipping boiler evaluation");
		return;
	}

	float diff = abs(temp1 - temp2);
	if (diff > VALVE_SWITCH_TEMPERATURE || activeBoiler == -1)
	{
		if (temp1 > temp2)
		{
			// Serial.println("warmer boiler 1");
			boilerToActivate = BOILER1_PIN;
		}
		else
		{
			// Serial.println("warmer boiler 2");
			boilerToActivate = BOILER2_PIN;
		}
	}
}

void activateBoiler()
{
	if (activeBoiler != boilerToActivate)
	{
		if (activeBoiler != -1)
		{
			digitalWrite(activeBoiler, LOW);
		}
		digitalWrite(boilerToActivate, HIGH);
		activeBoiler = boilerToActivate;
	}

	/*
	if (activeBoiler != boilerToActivate)
	{
		// Serial.println("activation in progress");
		activationInProgress = 1;
	}
	else
	{
		return;
	}

	if (boilerActivationTime == 0)
	{
		boilerActivationTime = rtc.now().unixtime();
	}

	uint32_t now = rtc.now().unixtime();
	uint32_t diff = now - boilerActivationTime;

	if (diff > VALVE_SWITCH_DURATION)
	{
		// Serial.print("Setting boiler pin to LOW - ");
		boilerActivationTime = 0;
		activeBoiler = boilerToActivate;
		activationInProgress = 0;
		digitalWrite(boilerToActivate, LOW);
	}
	else
	{
		// Serial.print("Setting boiler pin to HIGH - ");
		digitalWrite(boilerToActivate, HIGH);
	}
	// Serial.println(boilerToActivate);
	*/
}

const char *indexToDay(uint8_t index)
{
	switch (index)
	{
	case 0:
		return "Ned";
	case 1:
		return "Pon";
	case 2:
		return "Uto";
	case 3:
		return "Str";
	case 4:
		return "Stv";
	case 5:
		return "Pia";
	case 6:
		return "Sob";
	}
	return "ERR";
}

void evaluateHeating()
{
	DateTime dateTime = rtc.now();
	uint8_t day = dateTime.dayOfTheWeek();

	uint8_t hour = dateTime.hour();
	//	uint8_t hour = dateTime.minute(); // this is for testing

	if (day != 5 && day != 6 && day != 0)
	{
		digitalWrite(B2_HEATING_PIN, LOW);
		return;
	}
	if (hour < 3 || hour > 7)
	{
		digitalWrite(B2_HEATING_PIN, LOW);
		return;
	}

	digitalWrite(B2_HEATING_PIN, HIGH);
}

void calculateWaterLevel(DateTime now, Adafruit_ST7735 tft)
{
	int digitalInputValue = analogRead(A_IN_PIN);

	int position = currentMeasurement++ % mCount;
	measuredWaterLevels[position] = digitalInputValue;

	int sum = 0;
	for (int i = 0; i < mCount; i++)
	{
		sum += measuredWaterLevels[i];
	}
	int average = sum / mCount;

	writeWaterLevelText(4, average, 0, 71, ST77XX_CYAN);
	updateAverage(now, digitalInputValue, tft);
}

void writeWaterLevelText(int i2c_index, int digitalInput, int positionX, int positionY, uint16_t color)
{

	int16_t x, y;
	uint16_t w, h;
	char result[30];
	char textVoltage[10];
	char textDepth[10];

	float voltage = digitalInput * DA_RATIO; //- 0.075;
	dtostrf(voltage, 5, 3, textVoltage);
	float depth = ((voltage - 0.5) * 1000000) / (8 * WATER_DENSITY * GRAVITATIONAL_ACCELERATION);
	dtostrf(depth, 4, 2, textDepth);

	sprintf(result, "%s%sm  %sV  %d", previousTemperature[i2c_index][0], textDepth, textVoltage, digitalInput);

	int cmpTemp = scmp(previousTemperature[i2c_index][1], result);

	if (cmpTemp != 0)
	{
		tft.getTextBounds(previousTemperature[i2c_index][1], positionX, positionY, &x, &y, &w, &h);
		strcpy(previousTemperature[i2c_index][1], result);
		tft.setCursor(positionX, positionY);
		tft.fillRect(x, y, w, h, ST77XX_BLACK);
		tft.setTextColor(color);
		tft.print(result);
	}
}
