#ifndef STORAGE_DRIVER_H
#define STORAGE_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_types.h"

void StorageDriver_PreparePins(void);
void StorageDriver_Init(void);
bool StorageDriver_SavePreset(uint8_t preset, uint16_t pressure, uint16_t minutes);
uint16_t StorageDriver_BootLanguage(void);
uint16_t StorageDriver_ReadU16(uint8_t address, uint16_t default_value);
bool StorageDriver_WriteU16(uint8_t address, uint16_t value);
bool StorageDriver_ReadBytes(uint8_t address, uint8_t *data, size_t length);
bool StorageDriver_WriteBytes(uint8_t address, const uint8_t *data, size_t length);
bool StorageDriver_EraseMain(void);
void StorageDriver_IncrementCounter(AppMode mode);
void StorageDriver_IncrementEyeReplacement(void);
void StorageDriver_ScreenBootSync(void);

#endif
