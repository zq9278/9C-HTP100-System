#ifndef UI_PRESSURE_CALIBRATION_H
#define UI_PRESSURE_CALIBRATION_H
#include "../ui.h"
void ui_pressure_calibration_open(lv_event_t *e);
bool ui_pressure_calibration_active(void);
void ui_pressure_calibration_state(uint8_t state,bool authorized,bool pending,uint8_t result,uint16_t pressure);
#endif
