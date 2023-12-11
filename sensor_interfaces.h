#ifndef SENSOR_INTERFACES_H
#define SENSOR_INTERFACES_H

// Constants
extern const int waterLevelEmptyValue;
extern const int waterLevelFullValue;
extern const unsigned long waterLevelCheckInterval;
extern unsigned long lastWaterLevelCheck;

extern float manualVoltage;
extern bool overrideVoltage;
extern float manualWaterLevel;
extern bool overrideWaterLevel;

// Function Declarations
float readBatteryVoltage();
float sampleBattery();
float sampleWaterLevel();

#endif // SENSOR_INTERFACES_H
