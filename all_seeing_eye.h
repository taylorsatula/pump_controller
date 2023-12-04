#ifndef ALL_SEEING_EYE_H
#define ALL_SEEING_EYE_H

#include <ESP8266WebServer.h>

// Extern Declarations for Global Variables
extern float manualVoltage;
extern bool overrideVoltage;
extern float manualWaterLevel;
extern bool overrideWaterLevel;

extern const float voltageCutoff;

extern unsigned long backwashStartTime;
extern const unsigned long backwashDuration;
extern bool is_BackwashActive;

// ## Define server and port ## //
extern ESP8266WebServer server;

#endif // ALL_SEEING_EYE_H