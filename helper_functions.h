#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

// Function Declarations

// EEPROM Writing Functions
void writeEEPROM(int address, int value);
void writeEEPROM(int address, float value);
void writeEEPROM(int address, long value);
void writeEEPROM(int address, const String &value, int maxLen);

// EEPROM Reading Functions
int readEEPROM(int address, int defaultValue);
float readEEPROM(int address, float defaultValue);
long readEEPROM(int address, long defaultValue);
String readEEPROM(int address, String defaultValue, int maxLen);

// Utility Functions
int referenceValvePinSheet(const String& valveId);
void checkFilterStatus();
void updateFilterLifetimes(unsigned long pumpRuntimeSeconds);
void resetFilterLifetime(const String &filterType);
void resetTripMeter();

// Configuration Constants
extern const long max_LifetimeCarbonFilter;
extern const long max_LifetimeDiResin;
extern const long tripMeterTotal;

#endif // HELPER_FUNCTIONS_H
