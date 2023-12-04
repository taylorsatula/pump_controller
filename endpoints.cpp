#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include "control_surfaces.h"
#include "sensor_interfaces.h"
#include "all_seeing_eye.h"

// ## Filtration System ## //
unsigned long backwashStartTime = 0;
bool allow_Undervolting = false;
bool is_BackwashActive = false;



void handlePump() {
  if (!server.hasArg("plain")) {
    Serial.println(F("handle_pump: blank input error"));
    server.send(400, "application/json", F("{\"error\":\"invalid request\"}"));
    return;
  }

  const String receivedData = server.arg("plain");
  Serial.println("Received data: " + receivedData);

  StaticJsonDocument<256> doc; // Adjust size as needed
  DeserializationError error = deserializeJson(doc, receivedData);
  if (error) {
    Serial.println(F("JSON deserialization failed"));
    server.send(400, "application/json", F("{\"error\":\"invalid json\"}"));
    return;
  }

  float batteryVoltage = sampleBattery();
  bool is_BatteryHealthy = allow_Undervolting || batteryVoltage >= voltageCutoff;

  StaticJsonDocument<256> responseDoc;

  if (doc.containsKey("override")) {
    const char* override = doc["override"];
    allow_Undervolting = (strcmp(override, "on") == 0);
    responseDoc["override"] = override;
  }

  if (doc.containsKey("backwash")) {
    const char* backwashCommand = doc["backwash"];
    if (strcmp(backwashCommand, "begin") == 0 && is_BatteryHealthy) {
      is_BackwashActive = true;
      backwashStartTime = millis();
      runPump(true);
      Serial.println(F("Backwash started"));
      responseDoc["backwash"] = "started";
    } else {
      const char* errorKey = "error";
      const char* errorValue = is_BatteryHealthy ? "invalid command" : "backwash blocked due to low battery";
      Serial.println(errorValue);
      responseDoc[errorKey] = errorValue;
      String response;
      serializeJson(responseDoc, response);
      server.send(is_BatteryHealthy ? 400 : 200, "application/json", response);
      return;
    }
  }

  // Pump state
  if (doc.containsKey("pump")) {
    const char* state = doc["pump"];
    if (is_BatteryHealthy) {
      if (strcmp(state, "on") == 0) {
        runPump(true);
      } else if (strcmp(state, "off") == 0) {
        runPump(false);
      } else {
        Serial.println(F("handle_pump: invalid state"));
        responseDoc["error"] = "invalid state";
        String response;
        serializeJson(responseDoc, response);
        server.send(400, "application/json", response);
        return;
      }
    } else {
      Serial.println(F("handle_pump: blocked due to low battery"));
      responseDoc["error"] = "pump blocked due to low battery";
      String response;
      serializeJson(responseDoc, response);
      server.send(200, "application/json", response);
      return;
    }
    responseDoc["pump"] = state;
  }

  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}