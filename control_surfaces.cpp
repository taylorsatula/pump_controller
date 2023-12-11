#include <Arduino.h>
#include <EEPROM.h>

#include "pin_mapping.h"
#include "all_seeing_eye.h"
#include "helper_functions.h"
#include "endpoints.h"

// ## Pump ## //
unsigned long pumpStartTime = 0;
bool is_pumpRunning = false;


void controlValve(const String& valveId, bool state) {
  int valvePin = referenceValvePinSheet(valveId);
  if (state) digitalWrite(valvePin, LOW);
  else digitalWrite(valvePin, HIGH);
}


void runPump(bool state) {
  if (state && !is_pumpRunning) {
    digitalWrite(PIN_RELAY_PUMP, LOW);
    pumpStartTime = millis();
    is_pumpRunning = true;
  } else if (!state && is_pumpRunning) {
    digitalWrite(PIN_RELAY_PUMP, HIGH);
    unsigned long pumpRunDuration = millis() - pumpStartTime;
    is_pumpRunning = false;
    allow_Undervolting = false;

    long tripMeterTotal = readEEPROM(addr_TripMeter, (long)0);
    tripMeterTotal += pumpRunDuration / 1000; // Convert duration to seconds and add to total
    writeEEPROM(addr_TripMeter, tripMeterTotal);
    updateFilterLifetimes(pumpRunDuration / 1000);
  }
}
