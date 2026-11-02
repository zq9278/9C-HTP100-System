#ifndef SAFETY_MONITOR_H
#define SAFETY_MONITOR_H
#include <stdbool.h>
void SafetyMonitor_Init(void);
void SafetyMonitor_Service(bool motion_or_heat);
void SafetyMonitor_TickFromIsr(void);
bool SafetyMonitor_Tripped(void);
void SafetyMonitor_EmergencyOff(void);
#endif
