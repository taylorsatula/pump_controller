#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "pin_mapping.h"
#include "all_seeing_eye.h"

// ## Filter Replacement Config Values ## //
const long max_LifetimeCarbonFilter = 30 * 60 * 60; // 30 hours in seconds
const long max_LifetimeDiResin = 60 * 60 * 60;      // 60 hours
const long tripMeterTotal = 0;

void writeEEPROM(int address, int value) { EEPROM.put(address, value); Serial.println("EEPROM int written"); }
void writeEEPROM(int address, float value) { EEPROM.put(address, value); Serial.println("EEPROM float written"); }
void writeEEPROM(int address, long value) { EEPROM.put(address, value); Serial.println("EEPROM long written"); }

// For strings
void writeEEPROM(int address, const String &value, int maxLen) {
    int i;
    for (i = 0; i < maxLen && i < value.length(); i++) {
        EEPROM.write(address + i, value[i]);
    }
    EEPROM.write(address + i, '\0'); // Null-terminate the string
    Serial.println("EEPROM string written");
}

// For integers, floats, and longs
int readEEPROM(int address, int) { int value; EEPROM.get(address, value); return value; Serial.println("EEPROM int read"); }
float readEEPROM(int address, float) { float value; EEPROM.get(address, value); return value; Serial.println("EEPROM float read"); }
long readEEPROM(int address, long) { long value; EEPROM.get(address, value); return value; Serial.println("EEPROM long read"); }

// For strings
String readEEPROM(int address, String, int maxLen) {
    String value;
    for (int i = 0; i < maxLen; i++) {
        char ch = EEPROM.read(address + i);
        if (ch == '\0') break;
        value += ch;
    }
    return value;
    Serial.println("EEPROM string read");
}

int referenceValvePinSheet(const String& valveId) {
  if (valveId == "tankFill") {
    return PIN_RELAY_TANKVALVE;
  }
  
  return -1; // function must return something always
}

void checkFilterStatus() {
    long carbonLifetime = readEEPROM(addr_CarbonLifeRemaining, (long)0);
    long diResinLifetime = readEEPROM(addr_CarbonLifeRemaining, (long)0);

    if (carbonLifetime < 0) {
        Serial.println("Carbon filter is overdue for replacement.");
    } else {
        Serial.println("Carbon filter life remaining: " + String(carbonLifetime) + " seconds.");
    }

    if (diResinLifetime < 0) {
        Serial.println("DI Resin filter is overdue for replacement.");
    } else {
        Serial.println("DI Resin filter life remaining: " + String(diResinLifetime) + " seconds.");
    }
}

void updateFilterLifetimes(unsigned long pumpRuntimeSeconds) {
    long carbonLifetime = readEEPROM(addr_CarbonLifeRemaining, (long)0);
    long diResinLifetime = readEEPROM(addr_DiLifeRemaining, (long)0);

    carbonLifetime -= pumpRuntimeSeconds;
    diResinLifetime -= pumpRuntimeSeconds;

    writeEEPROM(addr_CarbonLifeRemaining, carbonLifetime);
    writeEEPROM(addr_DiLifeRemaining, diResinLifetime);

}

void resetFilterLifetime(const String &filterType) {
    if (filterType == "carbonFilter") {
        writeEEPROM(addr_CarbonLifeRemaining, max_LifetimeCarbonFilter);
    } else if (filterType == "diResin") {
        writeEEPROM(addr_DiLifeRemaining, max_LifetimeDiResin);
    }
    Serial.println("Filter lifetime reset: " + filterType);
}

void resetTripMeter() {
    writeEEPROM(addr_TripMeter, 0L);
    Serial.println("Trip meter reset to zero.");
}