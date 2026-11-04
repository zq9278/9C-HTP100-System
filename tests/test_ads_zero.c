#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "main.h"
#include "ads1220_driver.h"
#include "app_log.h"
SPI_HandleTypeDef hspi2;
static uint32_t tick;
static int32_t sample = 6000000;
static bool bcs, fail_read;
uint32_t HAL_GetTick(void) { return tick; }
void vTaskDelay(unsigned ms) { tick += ms; }
void HAL_GPIO_WritePin(int p,int n,int v) { (void)p;(void)n;(void)v; }
int HAL_GPIO_ReadPin(int p,int n) { (void)p;(void)n;return GPIO_PIN_RESET; }
int HAL_SPI_Transmit(SPI_HandleTypeDef *h,uint8_t *p,unsigned n,unsigned t) {
    (void)h;(void)t;if(n==2 && p[0]==0x44) bcs=(p[1]&1)!=0;return HAL_OK;
}
int HAL_SPI_Receive(SPI_HandleTypeDef *h,uint8_t *p,unsigned n,unsigned t) {
    (void)h;(void)t;
    if(fail_read)return HAL_ERROR;
    if(n==4) { uint8_t expected[]={0x3e,0x94,0x98,0};memcpy(p,expected,4); }
    else { int32_t value=bcs?1800000:sample;p[0]=(uint8_t)(value>>16);p[1]=(uint8_t)(value>>8);p[2]=(uint8_t)value; }
    return HAL_OK;
}
void AppLog_Write(const char *level,const char *fmt,...) { (void)level;(void)fmt; }
int main(void) {
    assert(Ads1220Driver_Init());
    assert(!Ads1220Driver_ZeroCancel());
    uint32_t before=tick;
    assert(Ads1220Driver_ZeroPoll(true)==ADS1220_RESULT_BUSY);
    assert(tick-before<10); /* No 500 ms blocking settle. */
    assert(Ads1220Driver_ZeroPoll(false)==ADS1220_RESULT_BUSY);
    tick+=500;
    for(int i=0;i<31;i++)assert(Ads1220Driver_ZeroPoll(false)==ADS1220_RESULT_BUSY);
    assert(Ads1220Driver_ZeroPoll(false)==ADS1220_RESULT_OK);
    assert(Ads1220Driver_ZeroValid()&&Ads1220Driver_ZeroRaw()==6000000);
    assert(Ads1220Driver_ZeroPoll(true)==ADS1220_RESULT_BUSY);
    tick+=500;sample=5900000;
    for(int i=0;i<12;i++)assert(Ads1220Driver_ZeroPoll(false)==ADS1220_RESULT_BUSY);
    assert(Ads1220Driver_ZeroCancel());
    assert(Ads1220Driver_ZeroValid()&&Ads1220Driver_ZeroRaw()==6000000);
    assert(Ads1220Driver_ZeroPoll(false)==ADS1220_RESULT_COMM);
    assert(!Ads1220Driver_ZeroCancel());
    assert(Ads1220Driver_ZeroPoll(true)==ADS1220_RESULT_BUSY);tick+=500;
    for(int i=0;i<32;i++) {
        sample=i<16?5500000:6000000;
        assert(Ads1220Driver_ZeroPoll(false)==(i==31?ADS1220_RESULT_ZERO_UNSTABLE:ADS1220_RESULT_BUSY));
    }
    assert(!Ads1220Driver_ZeroValid());
    assert(!Ads1220Driver_ZeroCancel());
    assert(Ads1220Driver_ZeroPoll(true)==ADS1220_RESULT_BUSY);tick+=500;fail_read=true;
    assert(Ads1220Driver_ZeroPoll(false)==ADS1220_RESULT_COMM);
    assert(!Ads1220Driver_ZeroValid());
    puts("PASS: cooperative ADC zero, unstable span sign, read failure");
}
