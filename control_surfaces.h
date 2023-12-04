#ifndef CONTROL_SURFACES_H
#define CONTROL_SURFACES_H

// Function Declarations
void controlValve(const String& valveId, bool state);
void runPump(bool state);

// extern bool is_TankFilling;
extern bool is_pumpRunning;

#endif // CONTROL_SURFACES_H
