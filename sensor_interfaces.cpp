#include <Arduino.h>
#include "pin_mapping.h"
#include "all_seeing_eye.h"


// ## Water Tank ## //
const int waterLevelEmptyValue = 0;
const int waterLevelFullValue = 1022;
unsigned long lastWaterLevelCheck = 0;
const unsigned long waterLevelCheckInterval = 5 * 1000; // 5 seconds




float readBatteryVoltage() {
  int sensorValue = analogRead(PIN_SENSOR_BATTERY);
  float voltage = sensorValue * (3.3 / 1023.0) * ((30000 + 10000) / 10000.0);
  // this part of the formula is used to scale the voltage reading back up to the battery's actual voltage.
  // this scaling is necessary because a voltage divider is used to step down the battery voltage to 
  // a level that is safe for the microcontroller's adc.
  return voltage;
}


float sampleBattery() {
  static int samplesPerCycle = 10;
  static int currentSampleCount = 0;
  static float totalVoltage = 0.0;
  static unsigned long lastSampleTime = 0;
  static float latestAverageVoltage = 0.0;

  const int sampleInterval = 100;

  if (overrideVoltage) {
    currentSampleCount = 0;
    totalVoltage = 0.0;
    return manualVoltage;
  }

  unsigned long currentTime = millis();
  if (currentTime - lastSampleTime >= sampleInterval) {
    lastSampleTime = currentTime;
    totalVoltage += readBatteryVoltage();
    currentSampleCount++;

    if (currentSampleCount == samplesPerCycle) {
      latestAverageVoltage = totalVoltage / samplesPerCycle;
      currentSampleCount = 0;
      totalVoltage = 0.0;
    }
  }
  return latestAverageVoltage;
}


float sampleWaterLevel() { // this function is very similar to voltage function
  static int samplespercycle = 50;   // 50 samples instead of 10
  static int currentsamplecount = 0; // this sensor measures static and (likely) needs to be way smoother
  static float totalreading = 0;     // tip: once the tool is here play with this for best speed vs. accuracy
  static unsigned long lastsampletime = 0;
  static float latestaveragewaterlevel = 0;

  const int sampleinterval = 100;

  if (overrideVoltage) {
    currentsamplecount = 0;
    totalreading = 0;
    latestaveragewaterlevel = manualWaterLevel; // this is bypassed bc it returns a variable set at a global level
    return manualWaterLevel;
  }

  unsigned long currenttime = millis();
  if (currenttime - lastsampletime >= sampleinterval) {
    lastsampletime = currenttime;
    totalreading += analogRead(PIN_SENSOR_WATERLEVEL);

    currentsamplecount++;

    if (currentsamplecount = samplespercycle) {
      int currentreading = totalreading / samplespercycle;
      int percentage = map(currentreading, waterLevelEmptyValue, waterLevelFullValue, 0, 100);
      latestaveragewaterlevel = percentage;

      currentsamplecount = 0;
      totalreading = 0;
    }
  }
  return latestaveragewaterlevel;
}