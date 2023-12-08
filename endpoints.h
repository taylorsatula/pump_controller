#ifndef ENDPOINTS_H
#define ENDPOINTS_H

// Function Declarations
void handlePump();
void handleValve();
void handleStatus();

extern unsigned long backwashStartTime;
extern bool allow_Undervolting;

#endif // ENDPOINTS_H
