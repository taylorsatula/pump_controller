#include <Arduino.h>
#include <ArduinoJson.h>
#include "control_surfaces.h"
#include "sensor_interfaces.h"
#include "helper_functions.h"
#include "all_seeing_eye.h"

// ## Filtration System ## //
unsigned long backwashStartTime = 0;
bool allow_Undervolting = false;
bool is_BackwashActive = false;
const unsigned long backwashDuration = 15 * 60 * 1000; // 15 minutes
unsigned long remainingBackwashDuration = 0;





void handlePump(String command) {
  Serial.println("Received data: " + command);

  StaticJsonDocument<1024> doc;  // Adjust size as needed
  DeserializationError error = deserializeJson(doc, command);
  if (error) {
    Serial.println(F("handlePump: JSON deserialization failed"));
    Serial.println(command);
    Serial.println(error.c_str());
    return;
  }

  float batteryVoltage = sampleBattery();
  bool is_BatteryHealthy = allow_Undervolting || batteryVoltage >= voltageCutoff;

  StaticJsonDocument<256> responseDoc;

  if (doc.containsKey("allow_Undervolting")) {
    const char* override = doc["allow_Undervolting"];
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
      const char* errorValue = is_BatteryHealthy ? "invalid command" : "backwash blocked due to low battery";
      Serial.println(errorValue);
      responseDoc["error"] = errorValue;
      String response;
      serializeJson(responseDoc, response);
      Serial.println(response);
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
        Serial.println(response);
        return;
      }
    } else {
      Serial.println(F("handle_pump: blocked due to low battery"));
      responseDoc["error"] = "pump blocked due to low battery";
      String response;
      serializeJson(responseDoc, response);
      Serial.println(response);
      return;
    }
    responseDoc["pump"] = state;
  }

  String response;
  serializeJson(responseDoc, response);
  Serial.println(response);
}


// void handleValve() {
//   if (!server.hasArg("plain")) {
//     Serial.println(F("handleValve: blank input error"));
//     server.send(400, "application/json", F("{\"error\":\"invalid request\"}"));
//     return;
//   }

//   const String receivedData = server.arg("plain");
//   Serial.println("Received data: " + receivedData);

//   StaticJsonDocument<1024> doc; // Size increased for multiple commands
//   DeserializationError error = deserializeJson(doc, receivedData);
//   if (error) {
//     Serial.println(F("JSON deserialization failed"));
//     server.send(400, "application/json", F("{\"error\":\"invalid json\"}"));
//     return;
//   }

//   StaticJsonDocument<1024> responseDoc;
//   JsonArray commands = doc.as<JsonArray>();

//   for (JsonVariant command : commands) {
//     if (!command.is<JsonObject>()) {
//       continue; // Skip if not an object
//     }

//     JsonObject commandObj = command.as<JsonObject>();

//     if (commandObj.containsKey("control") && commandObj.containsKey("valve")) {
//       String valveId = commandObj["control"];
//       String valveAction = commandObj["valve"];

//       int valvePin = referenceValvePinSheet(valveId);
//       if (valvePin == -1) {
//         Serial.print("Invalid valve ID: ");
//         Serial.println(valveId);
//         continue; // Skip to next command
//       }

//       bool state;
//       if (valveAction == "open") {
//         state = true;
//       } else if (valveAction == "close") {
//         state = false;
//       } else {
//         Serial.println(F("Invalid valve action"));
//         continue;
//       }

//       controlValve(valveId, state);

//       Serial.print("Valve ");
//       Serial.print(valveId);
//       Serial.print(" ");
//       Serial.println(valveAction);

//       // Add response for each valve action
//       JsonObject valveResponse = responseDoc.createNestedObject();
//       valveResponse["valve"] = valveId;
//       valveResponse["action"] = valveAction;
//     } else {
//       Serial.println(F("Invalid command format"));
//       continue;
//     }
//   }

//   String response;
//   serializeJson(responseDoc, response);
//   server.send(200, "application/json", response);
// }

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

void handleStatus(String request) {
  Serial.println("Received data: " + request);

  DynamicJsonDocument doc(2048);  // Adjust size as needed
  DeserializationError error = deserializeJson(doc, request);
  if (error) {
    Serial.print(F("handleStatus: JSON deserialization failed: "));
    Serial.println(error.c_str());
    Serial.println(request);
    return;
  }

  // Prepare response document
  DynamicJsonDocument responseDoc(1024);

  // Check if "status" key exists and get its value
  if (!doc.containsKey("status")) {
    Serial.println(F("handleStatus: 'status' key not found"));
    responseDoc["error"] = "status key not found";
    String response;
    serializeJson(responseDoc, response);
    Serial.println(response);
    return;
  }

  const char* statusType = doc["status"];  // Using const char* for reliable string comparison

  if (strcmp(statusType, "") == 0 || strcmp(statusType, "all") == 0) {  // Check for empty or "all"
    // Return all values
    responseDoc["allow_undervolting"] = allow_Undervolting;
    responseDoc["override_voltage"] = overrideVoltage;
    responseDoc["override_water_level"] = overrideWaterLevel;
    responseDoc["battery_voltage"] = sampleBattery();
    responseDoc["pump_status"] = is_pumpRunning;
    responseDoc["is_backwash_active"] = is_BackwashActive;
    responseDoc["remaining_backwash_duration"] = remainingBackwashDuration;
  }

  else if (strcmp(statusType, "allow_undervolting") == 0) {
    responseDoc["allow_undervolting"] = allow_Undervolting;
  } else if (strcmp(statusType, "override_voltage") == 0) {
    responseDoc["override_voltage"] = overrideVoltage;
  } else if (strcmp(statusType, "override_water_level") == 0) {
    responseDoc["override_water_level"] = overrideWaterLevel;
  } else if (strcmp(statusType, "battery_voltage") == 0) {
    responseDoc["battery_voltage"] = sampleBattery();
  } else if (strcmp(statusType, "water_level") == 0) {
    responseDoc["battery_voltage"] = sampleWaterLevel();
  } else if (strcmp(statusType, "pump_status") == 0) {
    responseDoc["pump_status"] = is_pumpRunning;
  } else if (strcmp(statusType, "is_backwash_active") == 0) {
    responseDoc["is_backwash_active"] = is_BackwashActive;
  } else if (strcmp(statusType, "remaining_backwash_duration") == 0) {
    responseDoc["remaining_backwash_duration"] = remainingBackwashDuration;
  } else {
    Serial.println(F("handleStatus: Unknown status request"));
    responseDoc["error"] = "unknown";
  }

  // Send response
  String response;
  serializeJson(responseDoc, response);
  Serial.println(response);
}
