#include "pressure_tuning.h"

#include <math.h>
#include <string.h>

#include "app_log.h"
#include "system_wire.h"

/* Boot loads validated calibration or compiled defaults; RTT overrides remain temporary. */
#ifndef PRESSURE_JLINK_TUNING_ENABLE
#define PRESSURE_JLINK_TUNING_ENABLE 0U
#endif
#define TUNING_USE_CODE_DEFAULTS (PRESSURE_JLINK_TUNING_ENABLE == 0U)
const volatile uint32_t g_pressure_jlink_tuning_enabled = PRESSURE_JLINK_TUNING_ENABLE;
/* 各档依次为：前进速度上限、回退速度上限、Kp、减速区间比例。 */
static const PressureTuningProfile g_defaults[PRESSURE_TUNING_PROFILE_COUNT] = {
    {20000.0f, 40000.0f, 100.0f, 0.9f},
    {20000.0f, 60000.0f, 200.0f, 0.9f},
    {40000.0f, 60000.0f, 100.0f, 0.9f},
    {45000.0f, 65000.0f,  50.0f, 0.9f},
    {45000.0f, 65000.0f,  50.0f, 0.9f}
};

static PressureTuningProfile g_profiles[PRESSURE_TUNING_PROFILE_COUNT];

static bool profile_valid(const PressureTuningProfile *profile)
{
    return profile != NULL &&
           isfinite(profile->fast_speed) &&
           isfinite(profile->retract_speed) &&
           profile->fast_speed > 0.0f && profile->fast_speed <= 65535.0f &&
           profile->retract_speed > 0.0f && profile->retract_speed <= 65535.0f &&
           isfinite(profile->kp) && profile->kp > 0.0f && profile->kp <= 1000.0f &&
           isfinite(profile->approach_ratio) &&
           profile->approach_ratio >= 0.01f && profile->approach_ratio <= 1.0f;
}

static PressureTuningProfile saved[PRESSURE_TUNING_PROFILE_COUNT];
static PressureRead storage_read;
static PressureWrite storage_write;
static bool persisted, storage_ok, leased;
static int active_bank = -1;
static uint16_t sequence;
/* 47-byte banks: 0x20..0x4E and 0x50..0x7E. Settings journal starts at 0x80. */
#define RECORD_SIZE 47U
static const uint8_t banks[2] = {0x20, 0x50};
void PressureTuning_SetLease(bool owned) { leased = owned; }
bool PressureTuning_Leased(void) { return leased; }
bool PressureTuning_Persisted(void) { return persisted; }
void PressureTuning_ResetDefaults(void) { memcpy(g_profiles, saved, sizeof(saved)); }
void PressureTuning_Init(void) {
    memcpy(saved, g_defaults, sizeof(saved));
    PressureTuning_ResetDefaults();
    storage_read = NULL; storage_write = NULL;
    persisted = storage_ok = leased = false; active_bank = -1; sequence = 0;
}
static void pack(uint8_t *p, const PressureTuningProfile *v) {
    Sw_Put16(p, (uint16_t)lroundf(v->fast_speed));
    Sw_Put16(p+2, (uint16_t)lroundf(v->retract_speed));
    Sw_Put16(p+4, (uint16_t)lroundf(v->kp*10));
    Sw_Put16(p+6, (uint16_t)lroundf(v->approach_ratio*1000));
}
static PressureTuningProfile unpack(const uint8_t *p) {
    PressureTuningProfile v = {Sw_Get16(p),Sw_Get16(p+2),Sw_Get16(p+4)/10.0f,Sw_Get16(p+6)/1000.0f};
    return v;
}
static bool valid_record(const uint8_t *r) {
    if (r[0]!=0x50 || r[1]!=0x43 || r[2]!=1 || Sw_Get16(r+45)!=Sw_Crc(r+2,43)) return false;
    for(unsigned i=0;i<5;i++) { PressureTuningProfile v=unpack(r+5+8*i); if(!profile_valid(&v))return false; }
    return true;
}
bool PressureTuning_Load(PressureRead read, PressureWrite write) {
    uint8_t a[RECORD_SIZE],b[RECORD_SIZE]; storage_read=read;storage_write=write;
    storage_ok=read && write && read(banks[0],a,sizeof(a)) && read(banks[1],b,sizeof(b));
    if(!storage_ok)return false;
    bool va=valid_record(a),vb=valid_record(b);
    if(va||vb) {
        active_bank=vb&&(!va||Sw_IsNewer(Sw_Get16(b+3),Sw_Get16(a+3)))?1:0;
        const uint8_t *r=active_bank?b:a;sequence=Sw_Get16(r+3);
        for(unsigned i=0;i<5;i++)saved[i]=unpack(r+5+8*i);
        persisted=true;
    }
    PressureTuning_ResetDefaults();return true;
}
const PressureTuningProfile *PressureTuning_ProfileForTarget(float target) {
    unsigned i=target<=200?0:target<=300?1:target<=400?2:target<=500?3:4;
    return &g_profiles[i];
}
bool PressureTuning_Get(uint8_t index, PressureTuningProfile *profile) {
    if(index>=5||!profile)return false;
    *profile=g_profiles[index];return true;
}
bool PressureTuning_Apply(uint8_t index,const PressureTuningProfile *profile) {
    if(index>=5||!profile_valid(profile)||leased)return false;
    uint8_t bytes[8];pack(bytes,profile);PressureTuningProfile quantized=unpack(bytes);
    if(!profile_valid(&quantized))return false;
    g_profiles[index]=quantized;return true;
}
bool PressureTuning_Set(uint8_t index,const PressureTuningProfile *profile) {
    if(TUNING_USE_CODE_DEFAULTS || index>=5 || !profile_valid(profile))return false;
    g_profiles[index]=*profile;return true;
}
bool PressureTuning_SaveProfile(uint8_t index,const PressureTuningProfile *profile) {
    if(!storage_ok || leased || index>=5 || !profile_valid(profile))return false;
    uint8_t r[RECORD_SIZE]={0x50,0x43,1},verify[RECORD_SIZE],zero[2]={0,0};
    Sw_Put16(r+3,(uint16_t)(sequence+1));
    for(unsigned i=0;i<5;i++)pack(r+5+8*i,i==index?profile:&saved[i]);
    Sw_Put16(r+45,Sw_Crc(r+2,43));if(!valid_record(r))return false;
    int bank=active_bank==0?1:0;uint8_t base=banks[bank];
    if(!storage_write(base,zero,2)||!storage_write(base+2,r+2,RECORD_SIZE-2)||
       !storage_write(base,r,2)||!storage_read(base,verify,sizeof(verify))||memcmp(r,verify,sizeof(r)))return false;
    saved[index]=unpack(r+5+8*index);g_profiles[index]=saved[index];
    active_bank=bank;sequence++;persisted=true;return true;
}
bool PressureTuning_Save(void) { return false; } /* RTT remains temporary. */
