#ifdef NDEBUG
#error Tests require enabled assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "system_wire.h"
#include "system_link.h"
#include "screen_link.h"
#include "pressure_motion.h"
#include "pressure_tuning.h"
#include "tmp112_sample.h"
#include "usart.h"
#define main original_controller_suite
#include "../firmware/main/tests/test_app_controller.c"
#undef main

UART_HandleTypeDef huart1;
uint32_t HAL_GetTick(void) { return fake_now; }
uint32_t HAL_GetUIDw0(void) { return 123; }
uint32_t HAL_GetUIDw1(void) { return 456; }
static uint8_t to_screen[4096], to_host[4096], rendered[SW_SNAPSHOT_BYTES];
static size_t screen_size, host_size;
static unsigned drop_snapshot, drop_result, drop_request;
static unsigned lost_calls;
static uint8_t rendered_result;
void screen_config_saved(void) {}
static uint8_t calibration_status;
static uint8_t calibration_memory[256];
static unsigned calibration_writes;
static bool cal_read(uint8_t a,uint8_t *p,size_t n){memcpy(p,calibration_memory+a,n);return true;}
static bool cal_write(uint8_t a,const uint8_t *p,size_t n){calibration_writes++;memcpy(calibration_memory+a,p,n);return true;}
void screen_calibration_result(uint8_t s,uint8_t o,uint8_t i,bool saved,const float *v){calibration_status=s;(void)o;(void)i;(void)saved;(void)v;}
bool Board_ScreenWrite(const uint8_t *p, uint16_t n)
{
    if (p[3] == SW_SNAPSHOT && drop_snapshot) { drop_snapshot--; return true; }
    if ((p[3] == SW_RESULT || p[3] == SW_CAL_RESULT) && drop_result) { drop_result--; return true; }
    assert(screen_size + n <= sizeof(to_screen)); memcpy(to_screen + screen_size,p,n);screen_size+=n;return true;
}
int HAL_UART_Transmit(UART_HandleTypeDef *u, const uint8_t *p, uint16_t n, uint32_t t)
{
    (void)u;(void)t;
    if (p[3] == SW_REQUEST && drop_request) { drop_request--; return HAL_OK; }
    assert(host_size+n<=sizeof(to_host)); memcpy(to_host+host_size,p,n);host_size+=n;return HAL_OK;
}
void screen_apply_system_state(const uint8_t *p, bool pending, uint8_t result)
{ (void)pending;rendered_result=result;memcpy(rendered,p,sizeof(rendered)); }
void screen_system_link_lost(void) { lost_calls++; }
static void exchange(void)
{
    ScreenLink_Tick();
    /* Deliberately fragmented delivery; both real parsers are under test. */
    for(size_t i=0;i<host_size;i++)SystemLink_Feed(to_host+i,1);host_size=0;
    SystemLink_Tick();
    for(size_t i=0;i<screen_size;i++)ScreenLink_Feed(to_screen+i,1);screen_size=0;
    ScreenLink_Tick();
}
static void advance(uint32_t ms) { fake_now += ms; AppController_Tick(); exchange(); }
static void test_engineering_link(void)
{
    reset_fixture(); SystemLink_Init(); ScreenLink_Init();
    for (unsigned i=0; i<5; i++) advance(200);
    assert(ScreenLink_Request(0x9201,250,0)); exchange();
    assert(rendered_result == SW_REJECTED);
    assert(ScreenLink_Request(0x9200,1,0)); exchange();
    assert(AppController_EngineeringMode() && (rendered[15] & 128));
    assert(!AppController_DebugMode()); /* Screen service mode does not bypass charging. */
    unsigned erased = storage_erase_count;
    drop_result=1; drop_snapshot=1;
    assert(ScreenLink_Request(0x9202,0,0)); exchange();
    advance(2500); exchange();
    assert(storage_erase_count == erased+1); /* Retried clear executes once. */
    /* Reconnect/enter if the intentionally delayed exchange expired the lease. */
    for (unsigned i=0;i<5;i++) advance(200);
    assert(ScreenLink_Request(0x9200,1,0)); exchange();
    storage_erase_ok=false;
    assert(ScreenLink_Request(0x9203,0,0)); exchange();
    assert(rendered_result == SW_REJECTED); storage_erase_ok=true;
    assert(ScreenLink_Request(0x9201,9999,120)); exchange();
    assert(rendered_result == SW_BAD_VALUE);
    drop_result=1; drop_snapshot=1;
    assert(ScreenLink_Request(0x9201,250,0)); exchange();
    unsigned starts = pressure_start_count;
    advance(300);
    assert(pressure_start_count == starts && AppController_Status()->state == APP_STATE_RUNNING);
    assert(AppController_Status()->remaining_ms == UINT32_MAX);
    assert(!AppController_Start()); /* Rejected START must not turn aging into a timed session. */
    for (unsigned i=0;i<900;i++) advance(200);
    assert(AppController_Status()->state == APP_STATE_RUNNING);
    assert(AppController_Status()->remaining_ms == UINT32_MAX);
    assert(ScreenLink_Request(0x9202,0,0)); exchange();
    assert(rendered_result == SW_BUSY);
    assert(ScreenLink_Request(0x9200,0,0)); exchange();
    assert(!AppController_EngineeringMode());
    assert(AppController_Status()->state != APP_STATE_RUNNING);
    advance(200);
    assert(ScreenLink_Request(0x9200,1,0)); exchange();
    assert(AppController_EngineeringMode());
    fake_now += SW_TIMEOUT_MS + 1; SystemLink_Tick();
    assert(!AppController_EngineeringMode());
    screen_size=host_size=0;
}
static void test_link(void)
{
    reset_fixture(); SystemLink_Init(); ScreenLink_Init();
    for(int i=0;i<5;i++)advance(200);
    assert(ScreenLink_Healthy());
    assert(ScreenLink_Request(0x1005,250,60)); exchange();
    assert(AppController_Status()->state==APP_STATE_READY);
    assert(rendered[12]==APP_STATE_READY);
    drop_result=1; drop_snapshot=1;
    assert(ScreenLink_Request(0x1006,0,0)); exchange();
    assert(AppController_Status()->state==APP_STATE_RUNNING);
    unsigned starts=pressure_start_count;
    advance(300);
    assert(!ScreenLink_Pending());
    assert(pressure_start_count==starts); /* Lost ACK cannot repeat actuation. */
    assert(rendered[12]==APP_STATE_RUNNING);
    drop_request=1;
    assert(ScreenLink_Request(0x1034,0,0)); exchange();
    assert(AppController_Status()->state==APP_STATE_RUNNING);
    advance(300);
    assert(AppController_Status()->state!=APP_STATE_RUNNING);
    assert(rendered[12]!=APP_STATE_RUNNING);
    advance(200);
    assert(ScreenLink_Request(0x1005,250,60));exchange();
    assert(ScreenLink_Request(0x1006,0,0));exchange();
    /* Delayed START must not reverse a later STOP. */

    assert(ScreenLink_Request(0x1034,0,0));exchange();advance(200);
    /* Request with a different command but an already-used sequence is rejected. */
    uint8_t req[SW_REQUEST_BYTES]={0}, wire[SW_MAX_FRAME];
    Sw_Put32(req,Sw_Get32(rendered));Sw_Put32(req+4,AppController_Status()->session_id);
    Sw_Put16(req+8,0x1006);
    size_t n=Sw_Encode(wire,SW_REQUEST,1,req,sizeof(req));SystemLink_Feed(wire,n);
    assert(AppController_Status()->state!=APP_STATE_RUNNING);
    assert(ScreenLink_Request(0x1005,250,60));exchange();
    assert(ScreenLink_Request(0x1006,0,0));exchange();
    /* Time expiry is local even when no UI finish command exists. */
    fake_now+=60000;AppController_Tick();
    assert(AppController_Status()->stop_reason==APP_STOP_NATURAL);
    assert(treatment_count==1);
}
static void test_local_deadline_and_bounds(void)
{
    reset_fixture(); AppController_SetRuntime(1);
    AppController_SetTemperature(INFINITY);AppController_SetPressure(INFINITY);
    assert(isfinite(AppController_Status()->settings.temperature_c));
    assert(AppController_Status()->settings.pressure_mmhg==350);
    assert(!AppController_Prepare(APP_MODE_PRESSURE,NAN));
    assert(!AppController_Prepare(APP_MODE_PRESSURE,701));
    assert(AppController_Prepare(APP_MODE_PRESSURE,250));
    fake_now=UINT32_MAX-1000U;assert(AppController_Start());
    fake_now+=59999U;AppController_Tick();assert(AppController_Status()->state==APP_STATE_RUNNING);
    fake_now++;AppController_Tick();assert(AppController_Status()->stop_reason==APP_STOP_NATURAL);
    reset_fixture();assert(AppController_Prepare(APP_MODE_PRESSURE,250));assert(AppController_Start());
    AppController_SetScreenLinkHealthy(false);assert(AppController_Status()->fault==APP_FAULT_SCREEN_TIMEOUT);
    AppController_Tick();AppController_SetScreenLinkHealthy(true);assert(AppController_Status()->state==APP_STATE_IDLE);
}
static void setup_running(void)
{
    host_size=screen_size=0;drop_request=drop_result=drop_snapshot=0;
    reset_fixture();SystemLink_Init();ScreenLink_Init();
    for(int i=0;i<5;i++)advance(200);
    assert(ScreenLink_Request(0x1005,250,60));exchange();
    assert(ScreenLink_Request(0x1006,0,0));exchange();
    assert(AppController_Status()->state==APP_STATE_RUNNING);
}
static void test_one_way_loss_and_reboots(void)
{
    setup_running();
    drop_request=100;
    assert(ScreenLink_Request(0x1034,0,0));
    for(int i=0;i<4;i++)advance(200);
    assert(AppController_Status()->state!=APP_STATE_RUNNING); /* STOP intent survives request-only loss. */
    setup_running();
    /* Only host->screen snapshots are lost. Screen still sends heartbeats. */
    drop_snapshot=100;
    for(int i=0;i<12;i++)advance(200);
    assert(AppController_Status()->fault==APP_FAULT_SCREEN_TIMEOUT);
    assert(AppController_Status()->state!=APP_STATE_RUNNING);
    drop_snapshot=0;
    for(int i=0;i<5;i++)advance(200);
    assert(AppController_Status()->state==APP_STATE_IDLE);
    assert(rendered[12]==APP_STATE_IDLE);
    setup_running();
    ScreenLink_Init(); /* UI reboot must not inherit a running session. */
    for(int i=0;i<6;i++)advance(200);
    assert(AppController_Status()->state!=APP_STATE_RUNNING);
    assert(rendered[12]!=APP_STATE_RUNNING);
    setup_running();
    reset_fixture();SystemLink_Init(); /* Main reboot invalidates the previous session. */
    for(int i=0;i<6;i++)advance(200);
    assert(rendered[12]==APP_STATE_IDLE);
    assert(!ScreenLink_Pending());
}
static void test_prepare_during_return_and_zero(void)
{
    for (unsigned natural = 0; natural < 2; natural++) {
        setup_running();
        fake_zero_busy = 8;
        if (natural) {
            fake_now += 60000;
            AppController_Tick();
            exchange();
        } else {
            assert(ScreenLink_Request(0x1034, 0, 0)); exchange();
        }
        unsigned starts = pressure_start_count;
        assert(AppController_Status()->state == APP_STATE_HOMING);
        /* Exercise motor return immediately, and zero calibration after home. */
        if (natural) advance(200);
        assert(AppController_Status()->state == APP_STATE_HOMING);
        assert(ScreenLink_CanPrepare());
        assert(pressure_start_count == starts);
        assert(ScreenLink_Request(0x1005, 250, 60)); exchange();
        assert(AppController_Status()->state == APP_STATE_READY);
        assert(ScreenLink_Request(0x1006, 0, 0)); exchange();
        assert(pressure_start_count == starts + 1);
    }
    /* Screen still sees IDLE while a physical-button session has already
     * stopped on the host: the old session must still be rejected as STALE. */
    host_size = screen_size = 0;
    reset_fixture(); SystemLink_Init(); ScreenLink_Init();
    for (unsigned i = 0; i < 5; i++) advance(200);
    assert(ScreenLink_CanPrepare());
    assert(AppController_Prepare(APP_MODE_PRESSURE, 250));
    assert(AppController_Start());
    AppController_Stop(APP_STOP_USER);
    assert(ScreenLink_Request(0x1005, 250, 60)); exchange();
    assert(rendered_result == SW_STALE);
    assert(!ScreenLink_Pending());
    advance(200);
    assert(ScreenLink_CanPrepare());
    assert(ScreenLink_Request(0x1005, 250, 60)); exchange();
    assert(rendered_result == SW_OK);
}
static unsigned parsed;
static void test_calibration_protocol(void) {
    reset_fixture();PressureTuning_Init();memset(calibration_memory,0xff,256);
    assert(PressureTuning_Load(cal_read,cal_write));SystemLink_Init();ScreenLink_Init();
    for(unsigned i=0;i<5;i++)advance(200);
    float v[4]={21000,42000,123.4f,.5f};
    assert(ScreenLink_RequestCalibration(0,1,NULL));exchange();assert(calibration_status==SW_REJECTED);
    assert(ScreenLink_Request(0x9200,1,0));exchange();
    assert(ScreenLink_RequestCalibration(0,1,NULL));exchange();assert(calibration_status==SW_OK);
    assert(ScreenLink_RequestCalibration(1,1,v));exchange();assert(calibration_status==SW_OK);
    PressureTuningProfile profile;assert(PressureTuning_Get(1,&profile));assert(fabsf(profile.kp-123.4f)<.01f);
    drop_result=1;unsigned before=calibration_writes;
    assert(ScreenLink_RequestCalibration(2,1,v));exchange();advance(300);
    assert(calibration_writes==before+3 && calibration_status==SW_OK);
    PressureTuning_ResetDefaults();assert(PressureTuning_Get(1,&profile));assert(fabsf(profile.kp-123.4f)<.01f);
    v[2]=NAN;assert(ScreenLink_RequestCalibration(1,1,v));exchange();assert(calibration_status==SW_BAD_VALUE);v[2]=200;
    assert(ScreenLink_Request(0x9204,250,0));exchange();assert(AppController_Status()->state==APP_STATE_RUNNING);
    assert(AppController_Status()->mode==APP_MODE_PRESSURE);
    assert(AppController_Status()->settings.pressure_mmhg==250);
    for(unsigned i=0;i<10;i++)advance(200);
    assert(last_heat_target<=0);
    assert(AppController_Status()->remaining_ms==UINT32_MAX);
    assert(ScreenLink_RequestCalibration(1,1,v));exchange();assert(calibration_status==SW_OK);
    assert(ScreenLink_RequestCalibration(2,1,v));exchange();assert(calibration_status==SW_BUSY);
    assert(ScreenLink_Request(0x1038,0,0));exchange();advance(200);
    assert(ScreenLink_Request(0x9200,0,0));exchange();assert(PressureTuning_Get(1,&profile));assert(fabsf(profile.kp-123.4f)<.01f);
}
static void test_battery_display_transients(void)
{
    reset_fixture();
    AppController_SetPower(false,false,100,4200);
    assert(AppController_Status()->battery_level == 100);
    for(unsigned i=0;i<10;i++) { fake_now+=100; AppController_SetPower(false,false,100,4000); }
    assert(AppController_Status()->battery_level == 100);
    for(unsigned i=0;i<100;i++) { fake_now+=100; AppController_SetPower(false,false,100,4200); }
    assert(AppController_Status()->battery_level >= 98);
    for(unsigned i=0;i<600;i++) { fake_now+=100; AppController_SetPower(false,false,100,3900); }
    unsigned low = AppController_Status()->battery_level;
    assert(low < 98 && low > 75);
    for(unsigned i=0;i<600;i++) { fake_now+=100; AppController_SetPower(false,false,100,4200); }
    assert(AppController_Status()->battery_level > low); /* Rebound is not permanently suppressed. */
    AppController_SetPower(false,false,100,3250);
    assert(AppController_Status()->low_battery_warning && AppController_Status()->battery_level <= 5);
    reset_fixture();
    AppController_SetEngineeringMode(true);
    assert(AppController_Prepare(APP_MODE_AUTO,250)); assert(AppController_StartAging());
    for(unsigned i=0;i<10;i++) AppController_SetPower(false,false,100,3150);
    assert(AppController_Status()->state != APP_STATE_RUNNING);
    assert(AppController_Status()->battery_shutdown_pending);
}
static void test_voltage_policy(void)
{
    reset_fixture();
    AppController_SetPower(false, false, 50, 3250);
    assert(AppController_Status()->low_battery_warning);
    AppController_SetPower(true, false, 50, 3250);
    assert(AppController_Status()->low_battery_warning);
    AppController_SetPower(true, false, 50, 3349);
    assert(AppController_Status()->low_battery_warning);
    AppController_SetPower(false, false, 50, 4200);
    assert(AppController_Status()->low_battery_warning);
    AppController_SetPower(true, false, 50, 3350);
    fake_now += 4999;
    AppController_SetPower(true, false, 50, 3350);
    assert(AppController_Status()->low_battery_warning);
    AppController_PowerReadFailed();
    fake_now += 1;
    AppController_SetPower(true, false, 50, 3350);
    assert(AppController_Status()->low_battery_warning);
    fake_now += 5000;
    AppController_SetPower(true, false, 50, 3350);
    assert(!AppController_Status()->low_battery_warning);
    reset_fixture();
    AppController_SetPower(false, false, 0, 3800);
    assert(!AppController_Status()->low_battery_warning);
    unsigned level = AppController_Status()->battery_level;
    AppController_SetPower(false, false, 100, 3800);
    assert(AppController_Status()->battery_level == level);
    assert(AppController_Prepare(APP_MODE_PRESSURE,250));assert(AppController_Start());
    AppController_SetPower(false, false, 100, 3251);
    assert(!AppController_Status()->low_battery_warning);
    AppController_SetPower(false, false, 100, 3250);
    assert(AppController_Status()->low_battery_warning);
    for(unsigned i=0;i<9;i++)AppController_SetPower(false,false,100,3150);
    assert(!AppController_Status()->battery_shutdown_pending);
    AppController_PowerReadFailed();
    AppController_SetPower(false,false,100,3150);
    assert(!AppController_Status()->battery_shutdown_pending);
    AppController_SetPower(false,false,100,3151); /* Above threshold breaks continuity. */
    for(unsigned i=0;i<9;i++)AppController_SetPower(false,false,100,3150);
    assert(!AppController_Status()->battery_shutdown_pending);
    AppController_SetPower(false,false,100,3150);
    assert(AppController_Status()->battery_shutdown_pending);
    assert(AppController_Status()->state == APP_STATE_RUNNING);
    assert(AppController_Status()->battery_level == 0);
    AppController_SetPower(false,false,100,3800);
    assert(AppController_Status()->battery_shutdown_pending);
    assert(!AppController_Prepare(APP_MODE_PRESSURE,250));
    AppController_Stop(APP_STOP_NATURAL);AppController_Tick();
    assert(AppController_Status()->state == APP_STATE_SHUTDOWN);
    assert(power_off_count == 1);
}
static void test_cooperative_zero(void)
{
    reset_fixture();
    AppController_Init(&port);
    fake_zero_busy = 8;
    for (int i = 0; i < 8; i++) {
        fake_now += 100;
        AppController_Tick();
        assert(AppController_Status()->state == APP_STATE_HOMING);
        assert(!AppController_Status()->pressure_zero_valid);
    }
    AppController_Tick();
    assert(AppController_Status()->state == APP_STATE_IDLE);
    assert(AppController_Status()->pressure_zero_valid);
    unsigned homes = home_begin_count;
    AppController_RaiseFault(APP_FAULT_CONTROL_STALE);
    assert(AppController_Status()->state == APP_STATE_FAULT);
    assert(!AppController_Status()->home_valid);
    AppController_Tick();
    assert(home_begin_count == homes);
    assert(!AppController_Prepare(APP_MODE_PRESSURE, 250));
}
static void got(uint8_t t,uint16_t seq,const uint8_t*p,uint8_t n){(void)p;assert(t==SW_REQUEST&&seq==99&&n==3);parsed++;}
static void test_wire_and_motion(void)
{
    float temperature = 99;
    assert(Tmp112_ConfigValid(0x60a0));assert(Tmp112_ConfigValid(0x6080));
    assert(!Tmp112_ConfigValid(0));assert(!Tmp112_ConfigValid(0xffff));
    assert(!Tmp112_ConfigValid(0x61a0));assert(!Tmp112_ConfigValid(0x60b0));
    assert(!Tmp112_Decode(0,&temperature));assert(temperature==99);
    assert(!Tmp112_Decode(0xffff,&temperature));assert(Tmp112_Decode(0x2830,&temperature));
    assert(fabsf(temperature-40.1875f)<0.001f);
    uint8_t frame[SW_MAX_FRAME],payload[]={1,2,3};size_t n=Sw_Encode(frame,SW_REQUEST,99,payload,3);
    for(size_t split=0;split<=n;split++) {SwParser p={0};unsigned before=parsed;Sw_Feed(&p,frame,split,got);Sw_Feed(&p,frame+split,n-split,got);assert(parsed==before+1);}
    SwParser p={0};uint8_t corrupt[SW_MAX_FRAME];memcpy(corrupt,frame,n);corrupt[8]^=0xff;
    unsigned before=parsed;Sw_Feed(&p,corrupt,n,got);Sw_Feed(&p,frame,n,got);assert(parsed==before+1&&p.bad_crc);
    uint32_t seed=1;for(unsigned i=0;i<50000;i++){seed=seed*1664525+1013904223;uint8_t b=(uint8_t)(seed>>24);Sw_Feed(&p,&b,1,got);assert(p.used<SW_MAX_FRAME);}
    assert(Sw_IsNewer(0,65535));assert(!Sw_IsNewer(65535,0));
    float before_switch=PressureMotion_Command(250,124.99f,20000,60000,200,.9f,false);
    float after_switch=PressureMotion_Command(250,125.01f,20000,60000,200,.9f,false);
    assert(fabsf(before_switch-after_switch)<10);
    assert(PressureMotion_Command(250,250,20000,60000,200,.9f,true)==0);
    assert(PressureMotion_Command(250,260,20000,60000,200,.9f,true)<0);
}
int main(void)
{
    original_controller_suite();test_engineering_link();test_cooperative_zero();test_wire_and_motion();test_local_deadline_and_bounds();test_link();test_one_way_loss_and_reboots();test_prepare_during_return_and_zero();test_voltage_policy();test_calibration_protocol();test_battery_display_transients();
    assert(PressureMotion_Slew(20000,0,.02f,100000)==0);
    assert(PressureMotion_Slew(0,20000,.02f,100000)==2000);
    puts("PASS: controller, protocol recovery, real host/client link, duplicate requests, timer rollover, motion continuity");
    return 0;
}
