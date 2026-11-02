#ifndef SYSTEM_LINK_H
#define SYSTEM_LINK_H
#include <stddef.h>
#include <stdint.h>
void SystemLink_Init(void);
void SystemLink_Feed(const uint8_t *data, size_t length);
void SystemLink_Tick(void);
#endif
