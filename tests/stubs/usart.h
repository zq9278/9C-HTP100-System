#include "main.h"
typedef struct { int unused; } UART_HandleTypeDef;
extern UART_HandleTypeDef huart1;
#define HAL_OK 0
#define HAL_ERROR 1
int HAL_UART_Transmit(UART_HandleTypeDef *uart, const uint8_t *data, uint16_t size, uint32_t timeout);
