#include "rtt_tuning.h"
#if PRESSURE_JLINK_TUNING_ENABLE
#include "SEGGER_RTT.h"
#include "system_wire.h"
#include "pressure_tuning.h"
#include "product_config.h"
#include "main.h"
#include <string.h>
static SwParser parser;
static uint8_t down[256];
static uint32_t owner, last_keepalive;
static void receive(uint8_t type,uint16_t seq,const uint8_t *p,uint8_t n)
{
    if(n<4)return;
    uint32_t token=Sw_Get32(p);
    uint8_t response[28]={type,SW_OK,0,0};
    Sw_Put32(response+4,token);Sw_Put32(response+24,PRODUCT_VERSION_NUMBER);
    if(type==0x10 && n==4 && token){
        if(owner && owner!=token) response[1]=SW_BUSY;
        else {owner=token;last_keepalive=HAL_GetTick();}
    }else if(!owner && type==0x14 && n==4 && token) { /* Lost reset ACK: already restored. */ }
    else if(!owner||token!=owner){response[1]=SW_STALE;}
    else {
        last_keepalive=HAL_GetTick();
        if(type==0x11 && n==4) { /* Lease heartbeat. */ }
        else if(type==0x14 && n==4){PressureTuning_ResetDefaults();owner=0;}
        else if((type==0x12&&n==5)||(type==0x13&&n==21)){
            PressureTuningProfile profile;uint8_t index=p[4];response[2]=index;
            if(type==0x13){
                memcpy(&profile,p+5,sizeof(profile));
                if(!PressureTuning_Set(index,&profile))response[1]=SW_BAD_VALUE;
            }
            if(!PressureTuning_Get(index,&profile))response[1]=SW_BAD_VALUE;
            else memcpy(response+8,&profile,sizeof(profile));
        }else response[1]=SW_BAD_VALUE;
    }
    uint8_t frame[SW_MAX_FRAME];size_t bytes=Sw_Encode(frame,0x15,seq,response,sizeof(response));
    (void)SEGGER_RTT_Write(0,frame,(unsigned)bytes);
}
void RttTuning_Init(void)
{
    SEGGER_RTT_Init();
    SEGGER_RTT_ConfigDownBuffer(0,"HTP100 tuning",down,sizeof(down),SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}
void RttTuning_Tick(void)
{
    if(owner&&HAL_GetTick()-last_keepalive>=2000U){PressureTuning_ResetDefaults();owner=0;}
    uint8_t bytes[64];unsigned n=SEGGER_RTT_Read(0,bytes,sizeof(bytes));
    Sw_Feed(&parser,bytes,n,receive);
}
#else
void RttTuning_Init(void) {}
void RttTuning_Tick(void) {}
#endif
