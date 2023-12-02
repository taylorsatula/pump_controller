#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <EasyDDNS.h>

// /////////////////////////// //
// VALUES FOR DEVELOPMENT ONLY //
// /////////////////////////// //

float manualVoltage = 0.0;
bool override_voltage = false;
float manualWaterLevel = 0;
bool override_waterLevel = false;

// /////////////////////////// //
//            PINS             //
// /////////////////////////// //

const int pumpRelayPin = 5; // Pump relay control is on pin 10 (PWM)
const int tankInputValveRelayPin = 4; // Tank valve relay is on pin 11 (PWM)
const int waterLevelSensorPin = A0; // Measures waterLevel in a 0-1023 value
const int batteryPin = A0; // Measure battery voltage on 12vdc circuit

// /////////////////////////// //
//        SERVICE VALUES       //
// /////////////////////////// //

const int addrCarbonFilter = 100;  // EEPROM address for carbon filter lifetime
const int addrDiResin = 200;       // EEPROM address for DI resin filter lifetime
const int addrTripMeter = 300;     // EEPROM address for the trip meter total
const long maxLifetimeCarbonFilter = 30 * 60 * 60;       // 30 hours in seconds
const long maxLifetimeDiResin =      60 * 60 * 60;      // 60 hours in seconds

// /////////////////////////// //
//     VALVE CONTROL VALUES    //
// /////////////////////////// //

const int waterLevelEmptyValue = 0;    // 
const int waterLevelFullValue = 1022; // Set measured value at full 0-1023 (TEST IRL)
bool isTankFilling = false; // default to false
unsigned long lastWaterLevelCheck = 0;
const unsigned long waterLevelCheckInterval =  5 * 1000; // 5 seconds

// /////////////////////////// //
//   BATTERY CONTROL VALUES    //
// /////////////////////////// //

const float voltageCutoff = 12.12; // (((((REVISE THIS WHEN I HAVE A BATTERY))))) Goal is 40% DoD
const int numBatterySamplesPerCycle = 10; // Number of samples to average
bool thisKillsTheCrab = false; // Prevent undervolting unless override is set // Initial value set to false 

// /////////////////////////// //
//     PUMP CONTROL VALUES     //
// /////////////////////////// //
bool isPumpRunning = false;

// /////////////////////////// //
//      BACKWASH CONTROL       //
// /////////////////////////// //

unsigned long pumpStartTime = 0;
bool inBackwashMode = false; // default to off
unsigned long backwashStartTime = 0;
const unsigned long backwashDuration = 1 * 60 * 1000; // 1 minute in milliseconds

// /////////////////////////// //
//       NETWORK VALUES        //
// /////////////////////////// //

const String wifi_ssid = "B.O.B.O.D.D.Y";
const String wifi_password = "Bhole#42069";
ESP8266WebServer server(2021);


// /////////////////////////// //
//             SETUP           //
// /////////////////////////// //

void setup() {
Serial.begin(921600);
// ^ Begin serial connection first and foremost ^
Serial.println("Device alive and serial connected");

  // Initialize persistant memory
    EEPROM.begin(512); // bytes
    Serial.println("EEPROM on");

  // Initialize the battery pin
    pinMode(batteryPin, INPUT);
    Serial.println("Battery pinmode Set");

  // Initialize the pump pin as an output
    pinMode(pumpRelayPin, OUTPUT);
    runPump(false); // Start with the pump off
    Serial.println("Pump pinmode Set");

  // Initialize the tank-input water value relay as an output
    pinMode(tankInputValveRelayPin, OUTPUT);
    openFillValve(false); // Start with the valve closed
    Serial.println("Tank pinmode Set");

  // Connect to Wi-Fi
     WiFi.begin(wifi_ssid, wifi_password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print("");
      Serial.print(".");
    }
    Serial.println("\nWiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    EasyDDNS.service("noip"); // Replace "noip" with your DDNS service name if different
    EasyDDNS.client("rcwc_pumpcontroller.ddns.net", "taylorsatula@gmail.com", "void0))Crypt.np"); // Use your DDNS credentials
    EasyDDNS.update(600000, true);
    Serial.println("EasyDDNS IP updated");

  // Define server endpoints
    server.on("/control_pump", HTTP_POST, handlePumpControl);
    server.on("/control_flow", HTTP_POST, handleWaterFlow);
    server.on("/service", HTTP_POST, handleService);
    server.on("/status", HTTP_GET, handleStatus);
    
  // Start the server
    server.begin();
  
  // Read existing filter lifespans. If no value is found initialize the maximum and write it.
    long carbonLifetime = readEEPROM(addrCarbonFilter, (long)0);
    long diResinLifetime = readEEPROM(addrDiResin, (long)0);
    long tripMeterTotal = readEEPROM(addrTripMeter, (long)0);

    if (carbonLifetime <= 0 || carbonLifetime > maxLifetimeCarbonFilter) {
        writeEEPROM(addrCarbonFilter, maxLifetimeCarbonFilter);
    }
    if (diResinLifetime <= 0 || diResinLifetime > maxLifetimeDiResin) {
        writeEEPROM(addrDiResin, maxLifetimeDiResin);
    }
    if (tripMeterTotal < 0) {  // Check for invalid data
        tripMeterTotal = 0;
        writeEEPROM(addrTripMeter, tripMeterTotal);
    }

    EEPROM.commit();
}


// /////////////////////////// //
//             LOOP            //
// /////////////////////////// //

void loop() {
    server.handleClient();
    
    float batteryVoltage = checkBattery(); // if the pump isn't turning on or is rapidly shutting back off we may
                                          // need to implement debounce logic irl. I believe a pump draws a lot more power briefly
                                         // when starting up. ymmv

    if (!thisKillsTheCrab && !inBackwashMode && batteryVoltage < voltageCutoff) {
        if (isPumpRunning) {
            runPump(false); // Turn off the pump due to low voltage
            Serial.println("Auto Shutdown: Pump turned off due to low battery");
            // Optionally, send an alert or perform another action
        }
    }

    if (inBackwashMode) {
        if (millis() - backwashStartTime >= backwashDuration) {
            inBackwashMode = false;
            runPump(false); // Stop the pump after 15 minutes
            Serial.println("Backwash complete!");
            // Optionally, send a completion notification
        }
    }

    if (isTankFilling && millis() - lastWaterLevelCheck > waterLevelCheckInterval) {
      lastWaterLevelCheck = millis();
      int currentWaterLevel = readWaterLevel();

      if (currentWaterLevel >= waterLevelFullValue) {
        openFillValve(false);
      }
    }
}


// /////////////////////////// //
//            BATTERY          //
// /////////////////////////// //

float readBatteryVoltage() {
    int sensorValue = analogRead(batteryPin);
    float voltage = sensorValue * (3.3 / 1023.0) * ((30000 + 10000) / 10000);
    return voltage;
}

float checkBattery() {
    if (override_voltage) {
        return manualVoltage;
    } else {
        float totalVoltage = 0.0;
        for (int i = 0; i < numBatterySamplesPerCycle; ++i) {
            totalVoltage += readBatteryVoltage();
            delay(100); // Small delay between readings
        }
        return totalVoltage / numBatterySamplesPerCycle;
    }
}

// /////////////////////////// //
//          fLOW CONTROL       //
// /////////////////////////// //

void handleWaterFlow() {
    if (server.hasArg("plain") == false) {
        Serial.println("handleWaterFlow: blank input error");
        server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
        return;
    }

    String requestData = server.arg("plain");
    Serial.println("Received Data: " + requestData);

    DynamicJsonDocument doc(1024);
    DynamicJsonDocument responseDoc(1024);
    deserializeJson(doc, requestData);

    if (doc.containsKey("valve")) {
        String valveCommand = doc["valve"];
        if (valveCommand == "open") {
            openFillValve(true);
            Serial.println("handleWaterFlow: opening");
            responseDoc["response"] = "Tank fill valve open";
        }
        else if (valveCommand == "close") {
            openFillValve(false);
            Serial.println("handleWaterFlow: closing");
            responseDoc["response"] = "Tank fill valve closing";
        } else {
            Serial.println("handleWaterFlow: invalid state");
            responseDoc["error"] = "Invalid valve state";
            String response;
            serializeJson(responseDoc, response);
            server.send(400, "application/json", response);
            return;
        }
    }
    else if (doc.containsKey("override")) {
        String override = doc["override"];
        override_waterLevel = (override == "on");
        responseDoc["response"] = override;
    } else {
        Serial.println("handleWaterFlow: invalid command");
        responseDoc["error"] = "Invalid command";
        String response;
        serializeJson(responseDoc, response);
        server.send(400, "application/json", response);
        return;
    }

    String response;
    serializeJson(responseDoc, response);
    server.send(200, "application/json", response);
}


int readWaterLevel() {
    if (override_waterLevel) {
        return manualWaterLevel;
    } else {
      long total = 0;
      int readings = 10;

      for (int i = 0; i < readings; i++) {
          total += analogRead(waterLevelSensorPin);
          delay(100); // Delay between readings
      }

      int averageReading = total / readings;
      int percentage = map(averageReading, waterLevelEmptyValue, waterLevelFullValue, 0, 100);
      percentage = constrain(percentage, 0, 100); // Ensure within 0-100%

      return percentage;
    }
}

void openFillValve(bool state) {
    if (state && !isTankFilling) {
        // Start the pump
        digitalWrite(tankInputValveRelayPin, HIGH);
        pumpStartTime = millis();
        isTankFilling = true;
    } else if (!state && isTankFilling) {
        // Stop the pump and update trip meter and filter lifetimes
        digitalWrite(tankInputValveRelayPin, LOW);
        isTankFilling = false;
    }
}

// /////////////////////////// //
//            PUMP             //
// /////////////////////////// //

void runPump(bool state) {
    if (state && !isPumpRunning) {
        // Start the pump
        digitalWrite(pumpRelayPin, HIGH);
        pumpStartTime = millis();
        isPumpRunning = true;
    } else if (!state && isPumpRunning) {
        // Stop the pump and update trip meter and filter lifetimes
        digitalWrite(pumpRelayPin, LOW);
        unsigned long pumpRunDuration = millis() - pumpStartTime;
        isPumpRunning = false;
        thisKillsTheCrab = false; // reset the override condition every time the pump turns off

        // Update Trip Meter
        long tripMeterTotal = readEEPROM(addrTripMeter, (long)0);
        tripMeterTotal += pumpRunDuration / 1000; // Convert duration to seconds and add to total
        writeEEPROM(addrTripMeter, tripMeterTotal);

        // Update Filter Lifetimes
        updateFilterLifetimes(pumpRunDuration / 1000); // Convert duration to seconds

        EEPROM.commit();
    }
}


void handlePumpControl() {
    if (server.hasArg("plain") == false) {
        Serial.println("handlePumpControl: blank input error");
        server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
        return;
    }

    String requestData = server.arg("plain");
    Serial.println("Received Data: " + requestData);
  
    DynamicJsonDocument doc(1024);
    DynamicJsonDocument responseDoc(1024);
    deserializeJson(doc, requestData);

    // Voltage Override
    if (doc.containsKey("override")) {
        String override = doc["override"];
        thisKillsTheCrab = (override == "on");
        responseDoc["override"] = override;
    }
  
    // Backwash Command
    if (doc.containsKey("backwash")) {
        String backwashCommand = doc["backwash"];
        float batteryVoltage = checkBattery();

        if (backwashCommand == "begin" && (thisKillsTheCrab || batteryVoltage >= voltageCutoff)) {
            inBackwashMode = true;
            backwashStartTime = millis();
            runPump(true);
            Serial.println("Backwash Started");
            responseDoc["backwash"] = "started";
        } else {
            if (batteryVoltage < voltageCutoff && !thisKillsTheCrab) {
                Serial.println("handlePumpControl: backwash blocked due to low battery");
                server.send(200, "application/json", "{\"error\":\"backwash blocked due to low battery\"}");
            } else {
                Serial.println("handlePumpControl: invalid backwash command");
                server.send(400, "application/json", "{\"error\":\"invalid backwash command\"}");
            }
            return;
        }
    }
  
    // Pump State
    if (doc.containsKey("pump")) {
        String state = doc["pump"];
        float batteryVoltage = checkBattery();

        if (thisKillsTheCrab || batteryVoltage >= voltageCutoff) {
            if (state == "on") {
                runPump(true);
                Serial.println("handlePumpControl: on");
            } else if (state == "off") {
                runPump(false);
                Serial.println("handlePumpControl: off");
            } else {
                Serial.println("handlePumpControl: invalid state");
                server.send(400, "application/json", "{\"error\":\"invalid state\"}");
                return;
            }
        } else {
            Serial.println("handlePumpControl: blocked due to low battery");
            server.send(200, "application/json", "{\"error\":\"pump blocked due to low battery\"}");
            return;
        }
        responseDoc["pump"] = state;
    }

    String response;
    serializeJson(responseDoc, response);
    server.send(200, "application/json", response);
}


// /////////////////////////// //
//            SERVICE          //
// /////////////////////////// //

void handleService() {
    if (server.hasArg("plain") == false) {
        Serial.println("handleService: blank input error");
        server.send(400, "application/json", "{\"error\":\"blank input\"}");
        return;
    }

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, server.arg("plain"));

    DynamicJsonDocument responseDoc(1024);

    // Handle Filter Reset Commands
    if (doc.containsKey("reset_filter")) {
        String resetCommand = doc["reset_filter"];
        if (resetCommand == "carbonFilter") {
            resetFilterLifetime("carbonFilter");
            Serial.println("handleService: carbon reset");
            responseDoc["carbonFilter_reset"] = true;
        }

        if (resetCommand == "diResin") {
            resetFilterLifetime("diResin");
            Serial.println("handleService: di reset");
            responseDoc["diResin_reset"] = true;
        }
    }
    else if (doc.containsKey("reset_tripmeter")) { // Handle Trip Meter Reset Command
        String resetTrip = doc["reset_tripmeter"];
        if (resetTrip == "true") {
            resetTripMeter();
            Serial.println("handleService: trip meter reset");
            responseDoc["response"] = "reset to zero";
        }
    } 
    else if (doc.containsKey("manualvoltage")) { // Manual Voltage Override
        manualVoltage = doc["manualvoltage"].as<float>();
        override_voltage = true;
        Serial.println("Manual voltage set to: " + String(manualVoltage));
        responseDoc["voltageSet"] = manualVoltage;
    }
    else if (doc.containsKey("manualWaterLevel")) {
        manualWaterLevel = doc["manualWaterLevel"].as<int>();
        override_waterLevel = true;
        Serial.println("Manual waterLevel set to: " + String(manualWaterLevel));
        responseDoc["waterLevelSet"] = manualWaterLevel;
    }

    String response;
    serializeJson(responseDoc, response);
    server.send(200, "application/json", response);
}


void updateFilterLifetimes(unsigned long pumpRuntimeSeconds) {
    long carbonLifetime = readEEPROM(addrCarbonFilter, (long)0);
    long diResinLifetime = readEEPROM(addrDiResin, (long)0);

    carbonLifetime -= pumpRuntimeSeconds;
    diResinLifetime -= pumpRuntimeSeconds;

    writeEEPROM(addrCarbonFilter, carbonLifetime);
    writeEEPROM(addrDiResin, diResinLifetime);

    EEPROM.commit();
}

void resetFilterLifetime(const String &filterType) {
    if (filterType == "carbonFilter") {
        writeEEPROM(addrCarbonFilter, maxLifetimeCarbonFilter);
    } else if (filterType == "diResin") {
        writeEEPROM(addrDiResin, maxLifetimeDiResin);
    }
    EEPROM.commit();
    Serial.println("Filter lifetime reset: " + filterType);
}

void resetTripMeter() {
    writeEEPROM(addrTripMeter, 0L);
    EEPROM.commit();
    Serial.println("Trip meter reset to zero.");
}

void checkFilterStatus() {
    long carbonLifetime = readEEPROM(addrCarbonFilter, (long)0);
    long diResinLifetime = readEEPROM(addrDiResin, (long)0);

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

// /////////////////////////// //
//            STATUS           //
// /////////////////////////// //

void handleStatus() {
    DynamicJsonDocument doc(1024);

    // Pump Status and Runtime
    doc["pump_running"] = isPumpRunning;
    doc["pump_runtime"] = isPumpRunning ? (millis() - pumpStartTime) / 1000 : 0; // Runtime in seconds
    
    // Current Pump Voltage
    doc["pump_voltage"] = checkBattery();

    // Backwash Status and Remaining Time
    if (inBackwashMode) {
        unsigned long elapsed = millis() - backwashStartTime;
        doc["backwashing"] = true;
        doc["backwash_remaining_time"] = (elapsed < backwashDuration) ? (backwashDuration - elapsed) / 1000 : 0; // Remaining time in seconds
    } else {
        doc["backwashing"] = false;
        doc["backwash_remaining_time"] = 0;
    }

    doc["tank_filling"] = isTankFilling;
    doc["waterLevel"] = readWaterLevel();

    // Override Status
    doc["override_voltage"] = thisKillsTheCrab;
    doc["override_tankFill"] = override_waterLevel;

    // Filter Lifetimes
    long carbonLifetime = readEEPROM(addrCarbonFilter, (long)0) / 3600; // Convert seconds to hours
    long diResinLifetime = readEEPROM(addrDiResin, (long)0) / 3600;
    doc["carbon_filter_lifetime"] = carbonLifetime;
    doc["di_resin_filter_lifetime"] = diResinLifetime;

    // Current Date and Time
    // Ensure NTP time is set up correctly to add date and time

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
    Serial.println("Staus Check Triggered");
}

// /////////////////////////// //
//            MISC             //
// /////////////////////////// //



// For integers, floats, and longs
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
