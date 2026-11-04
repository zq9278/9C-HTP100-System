#ifndef SCREEN_LINK_H
#define SCREEN_LINK_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
void ScreenLink_Init(void);
void ScreenLink_Feed(const uint8_t *data, size_t size);
void ScreenLink_Tick(void);
bool ScreenLink_Healthy(void);
bool ScreenLink_IsTreatmentCommand(uint16_t cmd);
bool ScreenLink_Request(uint16_t cmd, float value, uint16_t duration_s);
bool ScreenLink_RequestCalibration(uint8_t op,uint8_t index,const float *values);
void screen_calibration_result(uint8_t status,uint8_t op,uint8_t index,bool saved,const float *values);
bool ScreenLink_Pending(void);
uint16_t ScreenLink_LastCommand(void);
bool ScreenLink_CanPrepare(void);
/* Called only in the GUI/main loop, never from an interrupt. */
void screen_apply_system_state(const uint8_t *snapshot, bool pending, uint8_t result);
void screen_system_link_lost(void);
void screen_config_saved(void);
#endif
