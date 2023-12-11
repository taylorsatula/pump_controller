#ifndef ENDPOINTS_H
#define ENDPOINTS_H

// Function Declarations
void handlePump(String command);
// void handleValve();
void handleAdmin(String command);
void handleStatus(String request);

extern unsigned long backwashStartTime;
extern bool allow_Undervolting;

// # Backwash ## //
extern bool is_backwashActive;
extern unsigned long remainingBackwashDuration;

#endif  // ENDPOINTS_H
