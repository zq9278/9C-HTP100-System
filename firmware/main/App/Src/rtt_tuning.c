#include "rtt_tuning.h"
#if PRESSURE_JLINK_TUNING_ENABLE
#include "SEGGER_RTT.h"
#include "system_wire.h"
#include "pressure_tuning.h"
#include "product_config.h"
#include "main.h"
#include <string.h>
static SwParser parser;
static char logs[2048];
static uint32_t owner, last_keepalive;
volatile uint32_t g_rtt_recovery[8];
static void receive(uint8_t type,uint16_t seq,const uint8_t *p,uint8_t n)
{
    if(n<4)return;
    uint32_t token=Sw_Get32(p);
    uint8_t response[28]={type,SW_OK,0,0};
    Sw_Put32(response+4,token);Sw_Put32(response+24,PRODUCT_VERSION_NUMBER);
    if(type==0x10 && n==4 && token){
        if(owner && owner!=token) response[1]=SW_BUSY;
        else {owner=token;PressureTuning_SetLease(true);last_keepalive=HAL_GetTick();}
    }else if(!owner && type==0x14 && n==4 && token) { /* Lost reset ACK: already restored. */ }
    else if(!owner||token!=owner){response[1]=SW_STALE;}
    else {
        last_keepalive=HAL_GetTick();
        if(type==0x11 && n==4) { /* Lease heartbeat. */ }
        else if(type==0x14 && n==4){PressureTuning_ResetDefaults();PressureTuning_SetLease(false);owner=0;}
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
    SEGGER_RTT_ConfigUpBuffer(1,"HTP100 logs",logs,sizeof(logs),SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    /* Channel zero uses BUFFER_SIZE_DOWN; ConfigDownBuffer cannot resize it. */
    SEGGER_RTT_SetNameDownBuffer(0,"HTP100 tuning");
}
static bool ring_invalid(void)
{
    return _SEGGER_RTT.aUp[0].RdOff >= _SEGGER_RTT.aUp[0].SizeOfBuffer ||
           _SEGGER_RTT.aUp[0].WrOff >= _SEGGER_RTT.aUp[0].SizeOfBuffer ||
           _SEGGER_RTT.aUp[1].RdOff >= _SEGGER_RTT.aUp[1].SizeOfBuffer ||
           _SEGGER_RTT.aUp[1].WrOff >= _SEGGER_RTT.aUp[1].SizeOfBuffer ||
           _SEGGER_RTT.aDown[0].RdOff >= _SEGGER_RTT.aDown[0].SizeOfBuffer ||
           _SEGGER_RTT.aDown[0].WrOff >= _SEGGER_RTT.aDown[0].SizeOfBuffer;
}
void RttTuning_Tick(void)
{
    if(ring_invalid()) {
        g_rtt_recovery[0]++;
        g_rtt_recovery[1]=HAL_GetTick();
        g_rtt_recovery[2]=_SEGGER_RTT.aUp[0].RdOff;
        g_rtt_recovery[3]=_SEGGER_RTT.aUp[0].WrOff;
        g_rtt_recovery[4]=_SEGGER_RTT.aUp[1].RdOff;
        g_rtt_recovery[5]=_SEGGER_RTT.aUp[1].WrOff;
        g_rtt_recovery[6]=_SEGGER_RTT.aDown[0].RdOff;
        g_rtt_recovery[7]=_SEGGER_RTT.aDown[0].WrOff;
        /* Drop the diagnostic lease and partial frame, never continue using
         * out-of-range debug ring offsets. Treatment state is unchanged. */
        if(owner)PressureTuning_ResetDefaults();
        PressureTuning_SetLease(false);owner=0;memset(&parser,0,sizeof(parser));
        RttTuning_Init();
        return;
    }
    if(owner&&HAL_GetTick()-last_keepalive>=2000U){PressureTuning_ResetDefaults();PressureTuning_SetLease(false);owner=0;}
    uint8_t bytes[64];unsigned n=SEGGER_RTT_Read(0,bytes,sizeof(bytes));
    Sw_Feed(&parser,bytes,n,receive);
}
#else
void RttTuning_Init(void) {}
void RttTuning_Tick(void) {}
#endif
