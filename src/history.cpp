#include "history.h"

#include "generator.h"

#define COLOR_GRAY 0x83EF
#define COLOR_DARK_GRAY 0x4208

const int historySize = 64;
int history_data[historySize] = {-1};
float current_average = 0.0;
int measurements_count = 0;
uint32_t shiftInstant = 0;
int numberOfShifts = 0;

// ====== Drawing parameters ======
const int TOP_OF_CHART = 76;
const int SHIFT = 62;
const int min = 140;
const int max = 165;
const int step = 10;
// ================================

void updateAverage(DateTime now, int newVal, Adafruit_ST7735 tft)
{

  uint32_t secondsNow = now.secondstime();
  uint32_t modSeconds = secondsNow % (3600 * 6);
  // uint32_t modSeconds = secondsNow % (1); // for tuning only
  uint32_t newInstant = secondsNow - modSeconds;

  // INVESTIGATION
  // newVal = sinShrink();
  // newVal = 240;
  // shiftInstant = 0;
  ////////////////

  // printHistory();
  if (shiftInstant == 0 || shiftInstant != newInstant)
  {
    // Serial.println("---");
    shiftInstant = newInstant;
    shiftHistory();
    drawHistory(tft);
  }
  measurements_count++;
  current_average = current_average + ((newVal - current_average) / measurements_count);

  // Serial.print("avg: ");
  // Serial.println(current_average);
}

void shiftHistory()
{
  for (int i = 0; i < historySize - 1; i++)
  {
    history_data[i] = history_data[i + 1];
  }
  // Serial.print("avg: ");
  // Serial.println(current_average);
  history_data[historySize - 1] = round(current_average); // todo: is this correct rounding?
  current_average = 0.0;
  measurements_count = 0;
}

void printHistory()
{
  Serial.println("H: ");
  for (int i = 0; i < historySize; i++)
  {
    if (history_data[i] > -1)
    {
      Serial.print(history_data[i]);
      Serial.print(", ");
    }
  }
  Serial.println();
}

int measurementToPosition(int measurement)
{
  int position = (TOP_OF_CHART - measurement + SHIFT) * 2 + 127;
  // Serial.println(position);
  return position;
}

int positionToMeasurement(int p)
{
  int m = ((p - 127) / 2 - TOP_OF_CHART - SHIFT) * -1;
  // Serial.println(m);
  return m;
}

int findMinimalMeasuredValue()
{
  int min = 256;
  for (int i = historySize; i >= 0; i--)
  {
    if (history_data[i] < min && history_data[i] != 0)
    {
      min = history_data[i];
    }
  }
  return min;
}

int findMaximalMeasuredValue()
{
  int max = INT32_MIN;
  for (int i = historySize; i >= 0; i--)
  {
    if (history_data[i] > max && history_data[i] != 0)
    {
      max = history_data[i];
    }
  }
  return max;
}

int findMinimalMeasuredIndex()
{
  int min = historySize;
  for (int i = historySize; i >= 0; i--)
  {
    if (history_data[i] <= history_data[min] && history_data[i] != 0)
    {
      min = i;
    }
  }
  return min;
}

int findMaximalMeasuredIndex()
{
  int max = historySize;
  for (int i = historySize; i >= 0; i--)
  {
    if (history_data[i] >= history_data[max] && history_data[i] != 0)
    {
      max = i;
    }
  }
  return max;
}

void drawHistory(Adafruit_ST7735 tft)
{

  int minMeasuredValue = findMinimalMeasuredValue();
  int maxMeasuredValue = findMaximalMeasuredValue();

  int minMeasuredIndex = findMinimalMeasuredIndex();
  int maxMeasuredIndex = findMaximalMeasuredIndex();

  // Serial.print(minMeasuredValue);
  // Serial.print("m - ");
  // Serial.print(maxMeasuredValue);
  // Serial.print("m");
  // Serial.println();

  // int maxPositionInChart = measurementToPosition(maxMeasuredValue);
  // min value is at the top of the chart
  int minValueOfChart = 0; // = maxPositionInChart - (maxPositionInChart % 5) + 5;
  int maxValueOfChart = 0; // minValueOfChart + 25;

  if (maxMeasuredIndex > minMeasuredIndex)
  {
    // maxMeasuredValue
    // Serial.println("MAX");
    maxValueOfChart = maxMeasuredValue - (maxMeasuredValue % 5) + 5;
    minValueOfChart = maxValueOfChart - 25;
  }
  else
  {
    // minMeasuredValue
    // Serial.println("MIN");
    minValueOfChart = minMeasuredValue - (minMeasuredValue % 5);
    maxValueOfChart = minValueOfChart + 25;
  }

  // Serial.print(minValueOfChart);
  // Serial.print("ch - ");
  // Serial.print(maxValueOfChart);
  // Serial.println("ch");

  // Serial.print(measurementToPosition(minValueOfChart));
  // Serial.print("px - ");
  // Serial.print(measurementToPosition(maxValueOfChart));
  // Serial.println("px");

  int shiftBeforePrint = measurementToPosition(maxValueOfChart) - TOP_OF_CHART;
  Serial.println(shiftBeforePrint);

  tft.fillRect(0, TOP_OF_CHART - 3, 160, 132 - TOP_OF_CHART, ST7735_BLACK);
  drawChart(tft, minValueOfChart, maxValueOfChart, shiftBeforePrint);
  for (int i = historySize; i >= 0; i--)
  {
    int position = measurementToPosition(history_data[i]) - shiftBeforePrint;
    if (position >= TOP_OF_CHART)
    {
      tft.fillRect(1 + i * 2, position, 2, 2, ST7735_GREEN);
    }
  }
}

void drawChart(Adafruit_ST7735 tft, int minValueOfChart, int maxValueOfChart, int shiftBeforePrint)
{
  // Y-axis
  for (int i = minValueOfChart; i <= maxValueOfChart; i += 5)
  {
    if (i % 10 == 0)
    {
      tft.drawFastHLine(0, measurementToPosition(i) - shiftBeforePrint, 134, COLOR_GRAY);
      tft.setCursor(135, measurementToPosition(i) - shiftBeforePrint);
      if (i != maxValueOfChart)
      {
        tft.print(i);
      }
      // Serial.print("a - ");
    }
    else
    {
      tft.drawFastHLine(0, measurementToPosition(i) - shiftBeforePrint, 131, COLOR_DARK_GRAY);
      // Serial.print("b - ");
    }

    // Serial.print(i);
    // Serial.print("m: ");
    // Serial.print(measurementToPosition(i));
    // Serial.println("px");
  }

  // X-axis
  for (int i = 128; i >= 0; i -= 8)
  {
    tft.drawFastVLine(i, TOP_OF_CHART, 127 - TOP_OF_CHART, COLOR_DARK_GRAY);
  }
}