#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include "control_surfaces.h"
#include "sensor_interfaces.h"
#include "helper_functions.h"
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

void handleValve() {
  if (!server.hasArg("plain")) {
    Serial.println(F("handleValve: blank input error"));
    server.send(400, "application/json", F("{\"error\":\"invalid request\"}"));
    return;
  }

  const String receivedData = server.arg("plain");
  Serial.println("Received data: " + receivedData);

  StaticJsonDocument<1024> doc; // Size increased for multiple commands
  DeserializationError error = deserializeJson(doc, receivedData);
  if (error) {
    Serial.println(F("JSON deserialization failed"));
    server.send(400, "application/json", F("{\"error\":\"invalid json\"}"));
    return;
  }

  StaticJsonDocument<1024> responseDoc;
  JsonArray commands = doc.as<JsonArray>();

  for (JsonVariant command : commands) {
    if (!command.is<JsonObject>()) {
      continue; // Skip if not an object
    }

    JsonObject commandObj = command.as<JsonObject>();

    if (commandObj.containsKey("control") && commandObj.containsKey("valve")) {
      String valveId = commandObj["control"];
      String valveAction = commandObj["valve"];

      int valvePin = referenceValvePinSheet(valveId);
      if (valvePin == -1) {
        Serial.print("Invalid valve ID: ");
        Serial.println(valveId);
        continue; // Skip to next command
      }

      bool state;
      if (valveAction == "open") {
        state = true;
      } else if (valveAction == "close") {
        state = false;
      } else {
        Serial.println(F("Invalid valve action"));
        continue;
      }

      controlValve(valveId, state);

      Serial.print("Valve ");
      Serial.print(valveId);
      Serial.print(" ");
      Serial.println(valveAction);

      // Add response for each valve action
      JsonObject valveResponse = responseDoc.createNestedObject();
      valveResponse["valve"] = valveId;
      valveResponse["action"] = valveAction;
    } else {
      Serial.println(F("Invalid command format"));
      continue;
    }
  }

  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

// void handleService() {
//   // Check for valid request
//   if (!server.hasArg("plain")) {
//     Serial.println(F("handleFunctionName: blank input error"));
//     server.send(400, "application/json", F("{\"error\":\"invalid request\"}"));
//     return;
//   }

//   const String receivedData = server.arg("plain");
//   Serial.println("Received data: " + receivedData);

//   // Adjust size as needed
//   StaticJsonDocument<256> doc;
//   DeserializationError error = deserializeJson(doc, receivedData);
//   if (error) {
//     Serial.println(F("JSON deserialization failed"));
//     server.send(400, "application/json", F("{\"error\":\"invalid json\"}"));
//     return;
//   }

//   // Main logic starts here
//   // Replace this with your own processing logic
//   // Example: float someValue = processSomeData();
//   // Example: bool is_ConditionMet = checkSomeCondition();

//   // Prepare response document
//   StaticJsonDocument<256> responseDoc;

//   // Handle specific JSON keys and actions
//   // Example: if (doc.containsKey("someKey")) { /* Your code */ }

//   // Complete response preparation
//   // Example: responseDoc["resultKey"] = resultValue;

//   // Send response
//   String response;
//   serializeJson(responseDoc, response);
//   server.send(200, "application/json", response);
// }

void handleStatus() {
  // Check for valid request
  if (!server.hasArg("plain")) {
    Serial.println(F("handleStatus: blank input error"));
    server.send(400, "application/json", F("{\"error\":\"invalid request\"}"));
    return;
  }

  const String receivedData = server.arg("plain");
  Serial.println("Received data: " + receivedData);

  // Using dynamic document for flexibility
  DynamicJsonDocument doc(1024); // Adjust size as needed
  DeserializationError error = deserializeJson(doc, receivedData);
  if (error) {
    Serial.println(F("JSON deserialization failed"));
    server.send(400, "application/json", F("{\"error\":\"invalid json\"}"));
    return;
  }

  // Prepare response document
  DynamicJsonDocument responseDoc(1024);

  // Flag to determine if any specific key was requested
  bool specificKeyRequested = false;

  if (doc.containsKey("key1")) {
    specificKeyRequested = true;
    auto value = "hello!"; // Fetch value for key1
    responseDoc["key1"] = value;
  }

  if (doc.containsKey("key2")) {
    specificKeyRequested = true;
    auto value = "goodbye!"; // Fetch value for key1
    responseDoc["key_response"] = value;
  }

  // If no specific key requested, return all values
  if (!specificKeyRequested) {
    responseDoc["allow_undervolting"] = allow_Undervolting;
    responseDoc["override_voltage"] = overrideVoltage;
    responseDoc["override_waterlevel"] = overrideWaterLevel;
    responseDoc["allow_undervolting"] = allow_Undervolting;
    responseDoc["battery_voltage"] = sampleBattery();
    responseDoc["pump_status"] = is_pumpRunning;
  }

  // Send response
  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}
