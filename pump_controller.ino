// ## Includes Software ##
#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>


#include "all_seeing_eye.h"
#include "pin_mapping.h"
#include "creds.h"
#include "control_surfaces.h"
#include "sensor_interfaces.h"
#include "endpoints.h"
#include "helper_functions.h"


const int addr_TripMeter = 300;
const int addr_DiLifeRemaining = 200;
const int addr_CarbonLifeRemaining = 100;


// ## Battery ## //
const float voltageCutoff = 12.12;



void setup() {
  Serial.begin(9600); // converted this to a much slower rate so that it will play nicely with the future modem
  while (!Serial) { 
    ; // Wait for serial port to connect (necessary for native USB port only)
  }
  Serial.println("Device alive and Serial connected");

  // Initialize battery pin
  pinMode(PIN_SENSOR_BATTERY, INPUT);
  Serial.println("Battery sensor pin set");

  // Initialize pump control pin
  pinMode(PIN_RELAY_PUMP, OUTPUT);
  runPump(false);
  Serial.println("Pump relay pin set");

  // Initialize tank valve pin
  pinMode(PIN_RELAY_TANKVALVE, OUTPUT);
  // controlValve("tankFill", false);\
  Serial.println("Tank valve relay pin set");

  long carbonLifeRemaining = readEEPROM(addr_CarbonLifeRemaining, (long)0);
  long diLifeRemaining = readEEPROM(addr_DiLifeRemaining, (long)0);
  long tripMeterTotal = readEEPROM(addr_TripMeter, (long)0);

  if (tripMeterTotal < 0) {  // Check for invalid impossible data
    tripMeterTotal = 0;
    writeEEPROM(addr_TripMeter, tripMeterTotal);
  }
}

void processCommand(String commandJson) {
  // Check which command to execute and call the appropriate function
  if (commandJson.indexOf("\"pump\":") != -1) {
    handlePump(commandJson);
  } else if (commandJson.indexOf("\"backwash\":") != -1) {
    handlePump(commandJson);
  } else if (commandJson.indexOf("\"voltage\":") != -1) {
    handleAdmin(commandJson);
  } else if (commandJson.indexOf("\"water_level\":") != -1) {
    handleAdmin(commandJson);
  } else if (commandJson.indexOf("\"undervolt\":") != -1) {
    handleAdmin(commandJson);
  } else if (commandJson.indexOf("\"reset\":") != -1) {
    handleAdmin(commandJson);
  } else if (commandJson.indexOf("\"undervolt\":") != -1) {
    handleAdmin(commandJson);
  } else if (commandJson.indexOf("\"status\":") != -1) {
    handleStatus(commandJson);
  } else {
    Serial.println(F("Invalid command"));
  }
}



void loop() {
    float batteryVoltage = sampleBattery(); 
    // float waterLevel = sampleWaterLevel();

    if (!allow_Undervolting && !is_BackwashActive && batteryVoltage < voltageCutoff) {
        if (is_pumpRunning) {
            runPump(false); // Turn off the pump due to low voltage
            Serial.println("Auto Shutdown: Pump turned off due to low battery");
            // Optionally, send an alert or perform another action
        }
    }

    if (is_BackwashActive) {
      unsigned long elapsed = millis() - backwashStartTime;
      if (elapsed >= backwashDuration) {
        is_BackwashActive = false;
        runPump(false);
        Serial.println("Backwash complete!");
        remainingBackwashDuration = 0;
      } else {
        remainingBackwashDuration = (backwashDuration - elapsed) / 60000;
      }
    } else {
      remainingBackwashDuration = 0;
    }

    if (Serial.available()) {
      String receivedCommand = Serial.readStringUntil('\n');
      processCommand(receivedCommand);
    }
}
