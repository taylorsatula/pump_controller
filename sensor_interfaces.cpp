#include <Arduino.h>
// #include <NewPing.h>

#include "pin_mapping.h"
#include "all_seeing_eye.h"


// ## Water Tank ## //
const int waterLevelEmptyValue = 0;
const int waterLevelFullValue = 1022;
unsigned long lastWaterLevelCheck = 0;
const unsigned long waterLevelCheckInterval = 5 * 1000; // 5 seconds

float manualVoltage = 0.0;
bool overrideVoltage = false;
float manualWaterLevel = 0.0;
bool overrideWaterLevel = false;




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


// void sampleWaterLevel() {
//   current_WaterLevel = sonar.ping_in();
//   return current_waterLevel;
// }