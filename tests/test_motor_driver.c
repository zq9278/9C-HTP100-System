#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tmc5130_driver.h"
SPI_HandleTypeDef hspi1;
static bool enabled, tripped, configuring;
static uint32_t vmax, mode;
bool SafetyMonitor_Tripped(void) { return tripped; }
void AppLog_Write(const char *level,const char *fmt,...) { (void)level;(void)fmt; }
void HAL_GPIO_WritePin(int port,int pin,int value) {
    (void)port;if(pin==TMC_ENN_Pin)enabled=value==GPIO_PIN_RESET;
}
int HAL_SPI_Transmit(SPI_HandleTypeDef *h,uint8_t *p,unsigned n,unsigned t) {
    (void)h;(void)t;assert(n==5);
    if(configuring)assert(!enabled);
    uint32_t value=((uint32_t)p[1]<<24)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<8)|p[4];
    if(p[0]==0xa7)vmax=value;
    if(p[0]==0xa0)mode=value;
    return HAL_OK;
}
int HAL_SPI_TransmitReceive(SPI_HandleTypeDef *h,uint8_t *tx,uint8_t *rx,unsigned n,unsigned t) {
    (void)h;(void)t;assert(n==5);assert(tx[0]==4);
    memset(rx,0,n);rx[1]=0x11;return HAL_OK;
}
int main(void) {
    Tmc5130Driver_Init();assert(!enabled);
    Tmc5130Driver_Enable(true);assert(enabled);
    configuring=true;assert(Tmc5130Driver_Configure());configuring=false;
    assert(!enabled&&vmax==0);
    assert(Tmc5130Driver_SetSpeed(90000));assert(vmax==65535&&mode==2);
    assert(Tmc5130Driver_SetSpeed(-90000));assert(vmax==65535&&mode==1);
    assert(Tmc5130Driver_SetSpeed(0));assert(vmax==0);
    tripped=true;Tmc5130Driver_Enable(true);assert(!enabled);
    puts("PASS: motor reconfigure disabled, zero initial speed, speed bound, panic latch");
}
