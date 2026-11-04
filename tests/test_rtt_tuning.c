#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "rtt_tuning.h"
#include "pressure_tuning.h"
#include "system_wire.h"
static uint32_t now;static uint8_t incoming[64],reply[64];static size_t count;
uint32_t HAL_GetTick(void){return now;}
void SEGGER_RTT_Init(void){}
int SEGGER_RTT_ConfigUpBuffer(unsigned a,const char*b,void*c,unsigned d,unsigned e){assert(a==1);assert(d>=1024);(void)b;(void)c;(void)e;return 0;}
int SEGGER_RTT_ConfigDownBuffer(unsigned a,const char*b,void*c,unsigned d,unsigned e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
unsigned SEGGER_RTT_Read(unsigned a,void*b,unsigned n){(void)a;assert(count<=n);memcpy(b,incoming,count);unsigned r=(unsigned)count;count=0;return r;}
unsigned SEGGER_RTT_Write(unsigned a,const void*b,unsigned n){(void)a;memcpy(reply,b,n);return n;}
void AppLog_Write(const char*a,const char*b,...){(void)a;(void)b;}
bool StorageDriver_ReadBytes(uint8_t a,uint8_t*b,size_t n){(void)a;(void)b;(void)n;assert(0);return false;}
bool StorageDriver_WriteBytes(uint8_t a,const uint8_t*b,size_t n){(void)a;(void)b;(void)n;assert(0);return false;}
static void request(uint8_t type,uint32_t token,const PressureTuningProfile *p)
{
 uint8_t data[21]={0};Sw_Put32(data,token);data[4]=1;
 if(p)memcpy(data+5,p,sizeof(*p));
 count=Sw_Encode(incoming,type,1,data,p?21:4);RttTuning_Tick();
}
int main(void)
{
 PressureTuning_Init();RttTuning_Init();PressureTuningProfile original,changed,current;
 assert(PressureTuning_Get(1,&original));changed=original;changed.kp=55;
 request(0x13,123,&changed);assert(reply[8]==SW_STALE);
 request(0x10,123,NULL);assert(reply[8]==SW_OK);
 request(0x10,456,NULL);assert(reply[8]==SW_BUSY);
 request(0x13,123,&changed);assert(reply[8]==SW_OK);assert(PressureTuning_Get(1,&current)&&current.kp==55);
 changed.kp=NAN;request(0x13,123,&changed);assert(reply[8]==SW_BAD_VALUE);
 now=1999;RttTuning_Tick();assert(PressureTuning_Get(1,&current)&&current.kp==55);
 now=2000;RttTuning_Tick();assert(PressureTuning_Get(1,&current)&&current.kp==original.kp);
 request(0x13,123,&original);assert(reply[8]==SW_STALE);
 puts("PASS: RTT lease, exclusive owner, invalid profile rejection, disconnect defaults");
}
