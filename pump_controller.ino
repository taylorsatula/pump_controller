// ## Includes Software ##
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <EasyDDNS.h>
#include <EEPROM.h>

#include "all_seeing_eye.h"
#include "pin_mapping.h"
#include "creds.h"
#include "control_surfaces.h"
#include "sensor_interfaces.h"
#include "endpoints.h"
#include "helper_functions.h"

ESP8266WebServer server(2021);

const int addr_TripMeter = 300;
const int addr_DiLifeRemaining = 200;
const int addr_CarbonLifeRemaining = 100;

float manualVoltage = 0.0;
bool overrideVoltage = false;
float manualWaterLevel = 0.0;
bool overrideWaterLevel = false;

// ## Battery ## //
const float voltageCutoff = 12.12;

bool is_backwashActive;

const unsigned long backwashDuration = 15 * 60 * 1000; // 15 minutes


void setup() {
  Serial.begin(921600);
  Serial.println("Device alive and Serial connected");

  // Initialize persistent memory on ESP8266
  #ifdef ESP8266
    EEPROM.begin(512);
    Serial.println("EEPROM active");
  #endif

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

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Connect to and transmit to No-IP DDNS service
  EasyDDNS.service("noip");
  EasyDDNS.client(DDNS_HOSTNAME, DDNS_USERNAME, DDNS_PASSWORD);
  EasyDDNS.update(60000, true);  // This may need to be extended to reduce careless data usage
  Serial.println("EasyDDNS updated");

  server.begin();
  Serial.println("HTTP running");

  // Define server endpoints
  server.on("/control_pump", HTTP_POST, handlePump);
  // server.on("/control_flow", HTTP_POST, handleWater);
  // server.on("/service", HTTP_PATCH, handleService);
  // server.on("/status", HTTP_GET, handleStatus);


  long carbonLifeRemaining = readEEPROM(addr_CarbonLifeRemaining, (long)0);
  long diLifeRemaining = readEEPROM(addr_DiLifeRemaining, (long)0);
  long tripMeterTotal = readEEPROM(addr_TripMeter, (long)0);

  if (tripMeterTotal < 0) {  // Check for invalid impossible data
    tripMeterTotal = 0;
    writeEEPROM(addr_TripMeter, tripMeterTotal);
  }

  #ifdef ESP8266
    EEPROM.commit();
  #endif
}

void loop() {
    server.handleClient();
    
    float batteryVoltage = sampleBattery(); // if the pump isn't turning on or is rapidly shutting back off we may
                                          // need to implement debounce logic irl. I believe a pump draws a lot more power briefly
                                         // when starting up. ymmv

    if (!allow_Undervolting && !is_BackwashActive && batteryVoltage < voltageCutoff) {
        if (is_pumpRunning) {
            runPump(false); // Turn off the pump due to low voltage
            Serial.println("Auto Shutdown: Pump turned off due to low battery");
            // Optionally, send an alert or perform another action
        }
    }

    if (is_BackwashActive) {
        if (millis() - backwashStartTime >= backwashDuration) {
            is_BackwashActive = false;
            runPump(false); // Stop the pump after 15 minutes
            Serial.println("Backwash complete!");
            // Optionally, send a completion notification
        }
    }

    // if (is_TankFilling && millis() - lastWaterLevelCheck > waterLevelCheckInterval) {
    //   lastWaterLevelCheck = millis();
    //   int currentWaterLevel = sampleWaterLevel();

    //   if (currentWaterLevel >= waterLevelFullValue) {
    //     // openFillValve(false);
    //   }
    // }
}
