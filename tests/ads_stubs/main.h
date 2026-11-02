#pragma once
#include <stdint.h>
#define HAL_OK 0
#define HAL_ERROR 1
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define SPI2_CS_GPIO_Port 0
#define SPI2_CS_Pin 0
#define ADS1220_DRDY_GPIO_Port 0
#define ADS1220_DRDY_Pin 1
#define TMC_CSN_GPIO_Port 0
#define TMC_CSN_Pin 2
#define TMC_ENN_GPIO_Port 0
#define TMC_ENN_Pin 3
typedef int HAL_StatusTypeDef;
typedef struct { unsigned State, ErrorCode; } SPI_HandleTypeDef;
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(int port, int pin, int value);
int HAL_GPIO_ReadPin(int port, int pin);
int HAL_SPI_Transmit(SPI_HandleTypeDef *, uint8_t *, unsigned, unsigned);
int HAL_SPI_Receive(SPI_HandleTypeDef *, uint8_t *, unsigned, unsigned);
int HAL_SPI_TransmitReceive(SPI_HandleTypeDef *, uint8_t *, uint8_t *, unsigned, unsigned);
