#include "safety_monitor.h"
#include "main.h"
#include "app_config.h"
#include "treatment_hw.h"
static volatile uint32_t last_service;
static volatile bool armed, tripped, initialized;
void SafetyMonitor_EmergencyOff(void)
{
    /* No HAL locks, SPI, heap, RTOS or logging in a panic path. */
    RCC->IOPENR |= RCC_IOPENR_GPIOBEN | RCC_IOPENR_GPIODEN;
    (void)RCC->IOPENR;
    GPIOD->BSRR = TMC_ENN_Pin;
    GPIOD->MODER = (GPIOD->MODER & ~3UL) | 1UL;
    GPIOB->BSRR = (uint32_t)HEAT_CTRL_Pin << 16;
    GPIOB->MODER = (GPIOB->MODER & ~(3UL << 2)) | (1UL << 2);
    tripped = true;
}
void SafetyMonitor_Init(void)
{
    last_service = HAL_GetTick(); armed = false; initialized = true;
    /* ~8 s at nominal 32 kHz LSI; independent backstop when IRQs stop. */
    IWDG->KR = 0xcccc;
    IWDG->KR = 0x5555; IWDG->PR = 6; IWDG->RLR = 999;
    uint32_t started = HAL_GetTick();
    while (IWDG->SR != 0U) {
        if (HAL_GetTick() - started > 100U) { SafetyMonitor_EmergencyOff(); return; }
    }
    IWDG->KR = 0xaaaa;
}
void SafetyMonitor_Service(bool active)
{
    last_service = HAL_GetTick(); armed = active;
    IWDG->KR = 0xaaaa;
}
void SafetyMonitor_TickFromIsr(void)
{
    if (initialized && (armed || TreatmentHw_OutputsActive()) && TreatmentHw_OutputsActive() && !tripped && HAL_GetTick() - last_service > APP_CONTROL_DEADLINE_MS) SafetyMonitor_EmergencyOff();
}
bool SafetyMonitor_Tripped(void) { return tripped; }
