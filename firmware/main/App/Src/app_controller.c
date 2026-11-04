#include "app_controller.h"

#include <stddef.h>
#include <math.h>
#include <string.h>

#include "app_config.h"
#include "app_log.h"
#include "product_config.h"
#include "pressure_limit.h"
#include "pressure_tuning.h"

#define SCREEN_TEMP_HEAT             0x2041U
#define SCREEN_TEMP_AUTO             0x2037U
#define SCREEN_PRESSURE              0x2005U
#define SCREEN_PRESSURE_AUTO         0x2047U
#define SCREEN_SOC                   0x2050U
#define SCREEN_WORK_QUIT             0x2051U
#define SCREEN_TIMER_START           0x2052U
#define SCREEN_EYE_STATE             0x2055U
#define SCREEN_TIMER_STOP            0x2056U
#define SCREEN_NEW_EYE               0x2057U
#define SCREEN_HOME_COMPLETE         0x20F0U
#define SCREEN_FAULT_STATUS          0x2F01U

typedef struct {
    AppSnapshot status;
    const AppPort *port;
    uint32_t last_heat_ms;
    uint32_t last_pressure_ms;
    uint32_t last_temp_display_ms;
    uint32_t last_pressure_display_ms;
    uint32_t last_temp_log_ms;
    float display_temperature_c;
    bool display_temperature_valid;
    bool tmp112_recovery_confirmed;
    bool eye_link_healthy;
    bool eye_link_paused_pressure;
    uint32_t temp_comm_failure_started_ms;
    bool temp_comm_failure_timing;
    uint32_t over_temperature_started_ms;
    bool over_temperature_timing;
    PressureLimit pressure_limit;
    uint8_t low_voltage_samples;
    bool warning_recovery_active;
    uint32_t warning_recovery_started_ms;
    bool count_recorded;
    bool pending_power_off;
    bool home_started;
    bool zero_pending;
    bool quick_resume_active;
    bool reusable_pressure_zero;
    bool power_state_logged;
    bool debug_mode;
    bool engineering_mode;
    bool aging_running;
    bool battery_display_initialized;
    float battery_filtered_mv;
    uint32_t battery_display_sample_ms, battery_display_change_ms;
    int8_t battery_display_direction;
    bool screen_link_healthy;
    uint32_t treatment_started_ms;
    uint32_t treatment_duration_ms;
} AppContext;

static AppContext g_app;

static uint32_t now_ms(void)
{
    return (g_app.port != NULL && g_app.port->now_ms != NULL) ? g_app.port->now_ms() : 0U;
}

static bool mode_uses_heat(AppMode mode)
{
    return mode == APP_MODE_HEAT || mode == APP_MODE_AUTO;
}

static bool mode_uses_pressure(AppMode mode)
{
    return mode == APP_MODE_PRESSURE || mode == APP_MODE_AUTO;
}

static bool eye_is_screen_online(AppEyeState eye)
{
    return eye == APP_EYE_NEW || eye == APP_EYE_IN_USE ||
           eye == APP_EYE_SERVICE;
}

static void report_fault_if_eye_online(AppFault fault)
{
    if (!eye_is_screen_online(g_app.status.eye)) {
        LOGW("[Screen TX] Fault suppressed while eye is offline: 0x%08lX",
             (unsigned long)fault);
        return;
    }
    if (g_app.port != NULL && g_app.port->fault_report != NULL) {
        g_app.port->fault_report(fault);
    }
}

static void clear_screen_fault_before_eye_offline(void)
{
#if APP_ENABLE_SCREEN_FAULT_REPORT
    if (g_app.port != NULL && g_app.port->screen_u32 != NULL) {
        /* Clear a previously displayed fault first so 0x2055 offline becomes
         * the screen's only active popup. */
        g_app.port->screen_u32(SCREEN_FAULT_STATUS, 0U);
    }
#endif
}

static bool treatment_is_open(void)
{
    return g_app.status.state == APP_STATE_PREHEAT ||
           g_app.status.state == APP_STATE_READY ||
           g_app.status.state == APP_STATE_RUNNING;
}

static bool stop_allows_quick_resume(AppStopReason reason)
{
    return reason == APP_STOP_USER || reason == APP_STOP_NATURAL;
}

static void send_float(uint16_t command, float value)
{
    if (g_app.port != NULL && g_app.port->screen_float != NULL) {
        g_app.port->screen_float(command, value);
    }
}

static void set_led(void)
{
    AppLedState led = APP_LED_IDLE;
    bool blink = false;

    if (g_app.port == NULL || g_app.port->led_set == NULL) {
        return;
    }
    if (g_app.status.charging && g_app.status.charge_full) {
        led = APP_LED_FULL;
    } else if (g_app.status.charging) {
        led = APP_LED_CHARGING;
    } else if (g_app.status.fault != APP_FAULT_NONE) {
        led = APP_LED_FAULT;
    } else if (g_app.status.low_battery_warning) {
        led = APP_LED_WARNING;
        blink = true;
    }
    g_app.port->led_set(led, blink);
}

static void request_yellow_warning_flash(void)
{
    if (!g_app.status.charging &&
        g_app.port != NULL && g_app.port->led_set != NULL) {
        /* APP_LED_FAULT starts the WS2812 driver's three-flash yellow pattern.
         * A following set_led() changes the steady state without cancelling it. */
        g_app.port->led_set(APP_LED_FAULT, false);
    }
}

static void try_clear_tmp112_fault(void)
{
    if (!g_app.tmp112_recovery_confirmed ||
        g_app.status.fault != APP_FAULT_TMP112_COMM ||
        g_app.status.state != APP_STATE_FAULT || !g_app.status.home_valid) {
        return;
    }
    g_app.tmp112_recovery_confirmed = false;
    g_app.status.fault = APP_FAULT_NONE;
    g_app.status.mode = APP_MODE_NONE;
    g_app.status.state = APP_STATE_IDLE;
    LOGI("[Fault] TMP112 communication recovered; fault cleared eye=%u",
         (unsigned)g_app.status.eye);
    set_led();
}

static void safe_outputs_off(void)
{
    PressureLimit_Reset(&g_app.pressure_limit);
    if (g_app.port == NULL) {
        return;
    }
    if (g_app.port->safe_outputs_off != NULL) {
        g_app.port->safe_outputs_off();
    }
    if (g_app.port->pressure_control_stop != NULL) {
        g_app.port->pressure_control_stop();
    }
}

static void record_natural_finish_once(void)
{
    if (g_app.count_recorded || g_app.status.stop_reason != APP_STOP_NATURAL) {
        return;
    }
    g_app.count_recorded = true;
    if (g_app.port != NULL && g_app.port->counter_increment != NULL) {
        g_app.port->counter_increment(g_app.status.mode);
    }
}

static void begin_home(void)
{
    LOGI("[Motor] Homing started");
    g_app.reusable_pressure_zero =
        stop_allows_quick_resume(g_app.status.stop_reason) &&
        g_app.status.pressure_zero_valid;
    g_app.quick_resume_active = false;
    g_app.status.state = APP_STATE_HOMING;
    g_app.status.home_valid = false;
    g_app.status.pressure_zero_valid = false;
    g_app.status.temperature_valid = false;
    g_app.temp_comm_failure_timing = false;
    g_app.home_started = false;

    g_app.zero_pending = false;

    if (g_app.port != NULL && g_app.port->home_begin != NULL && g_app.port->home_begin()) {
        g_app.home_started = true;
    } else {
        LOGE("[Motor] Homing start failed");
        g_app.status.fault = APP_FAULT_MOTOR_COMM;
        g_app.status.state = APP_STATE_FAULT;
        report_fault_if_eye_online(APP_FAULT_MOTOR_COMM);
        if (g_app.pending_power_off && g_app.port != NULL && g_app.port->power_latch_off != NULL) {
            g_app.port->power_latch_off();
        }
    }
}

static bool consume_eye_at_first_actuation(void)
{
    if (g_app.status.eye == APP_EYE_SERVICE || g_app.status.eye == APP_EYE_IN_USE) {
        return true;
    }
    if (g_app.status.eye != APP_EYE_NEW) {
        AppController_RaiseFault(APP_FAULT_TMP112_COMM);
        return false;
    }
#if PRODUCT_EYE_FUSE_ENABLED
    if (g_app.port == NULL || g_app.port->eye_mark_consumed == NULL ||
        !g_app.port->eye_mark_consumed()) {
        AppController_RaiseFault(APP_FAULT_TMP112_COMM);
        return false;
    }
#endif
    /* Keep the current uninterrupted insertion session usable. */
    g_app.status.eye = APP_EYE_IN_USE;
    /* SCREEN_NEW_EYE is an insertion event. It was already sent when the
     * eye shield changed to APP_EYE_NEW, so do not send it again at start. */
    return true;
}

void AppController_Init(const AppPort *port)
{
    memset(&g_app, 0, sizeof(g_app));
    g_app.port = port;
    g_app.status.state = APP_STATE_BOOT;
    g_app.status.mode = APP_MODE_NONE;
    g_app.status.eye = APP_EYE_ABSENT;
    g_app.status.settings.temperature_c = APP_DEFAULT_TREATMENT_TEMP_C;
    g_app.status.settings.pressure_mmhg = 350.0f;
    g_app.status.settings.runtime_minutes = 1U;
    g_app.eye_link_healthy = true;
    g_app.screen_link_healthy = true;
    safe_outputs_off();
    begin_home();
    set_led();
    LOGI("[App] Controller initialized");
}

bool AppController_CanQuickResume(void)
{
    return g_app.status.state == APP_STATE_HOMING &&
           stop_allows_quick_resume(g_app.status.stop_reason) &&
           g_app.status.fault == APP_FAULT_NONE && !g_app.pending_power_off && !g_app.status.battery_shutdown_pending &&
           g_app.reusable_pressure_zero && (g_app.home_started || g_app.zero_pending);
}

bool AppController_Prepare(AppMode mode, float pressure_mmhg)
{
    bool eye_valid = g_app.status.eye == APP_EYE_NEW ||
                     g_app.status.eye == APP_EYE_IN_USE ||
                     g_app.status.eye == APP_EYE_SERVICE;
    bool quick_request = g_app.status.state == APP_STATE_HOMING &&
                         stop_allows_quick_resume(g_app.status.stop_reason);

    if (!g_app.screen_link_healthy || mode < APP_MODE_HEAT || mode > APP_MODE_AUTO ||
        (mode_uses_pressure(mode) && (!isfinite(pressure_mmhg) || pressure_mmhg <= 0.0f || pressure_mmhg > APP_MAX_TARGET_PRESSURE_MMHG)) ||
        (g_app.status.charging && !g_app.debug_mode) ||
        g_app.status.fault != APP_FAULT_NONE || g_app.pending_power_off || g_app.status.battery_shutdown_pending || !eye_valid ||
        (!quick_request &&
         (g_app.status.state != APP_STATE_IDLE || !g_app.status.home_valid))) {
        LOGW("[App] Prepare rejected: state=%u mode=%u eye=%u charging=%u fault=0x%04X home=%u",
             (unsigned)g_app.status.state, (unsigned)mode, (unsigned)g_app.status.eye,
             g_app.status.charging ? 1U : 0U, (unsigned)g_app.status.fault,
             g_app.status.home_valid ? 1U : 0U);
        return false;
    }
    if (quick_request) {
        if ((!g_app.home_started && !g_app.zero_pending) || g_app.port == NULL ||
            g_app.port->home_cancel == NULL ||
            (mode_uses_pressure(mode) && !g_app.reusable_pressure_zero)) {
            LOGW("[App] Quick resume rejected: home_started=%u zero_reusable=%u mode=%u",
                 g_app.home_started ? 1U : 0U,
                 g_app.reusable_pressure_zero ? 1U : 0U, (unsigned)mode);
            return false;
        }
        if (g_app.zero_pending && (g_app.port->pressure_zero_cancel == NULL ||
            !g_app.port->pressure_zero_cancel())) return false;
        g_app.zero_pending = false;
        g_app.port->home_cancel();
        g_app.home_started = false;
        g_app.quick_resume_active = true;
        g_app.status.pressure_zero_valid = g_app.reusable_pressure_zero;
        g_app.status.state = APP_STATE_IDLE;
        LOGW("[App] Quick resume accepted before home; previous pressure zero reused");
    } else {
        g_app.quick_resume_active = false;
    }
    if (mode_uses_pressure(mode) && !g_app.status.pressure_zero_valid) {
        LOGW("[App] Prepare rejected: pressure zero invalid");
        return false;
    }

    g_app.status.session_id++;
    g_app.status.remaining_ms = (uint32_t)g_app.status.settings.runtime_minutes * 60000U;
    g_app.status.mode = mode;
    g_app.status.stop_reason = APP_STOP_NONE;
    g_app.status.fault = APP_FAULT_NONE;
    g_app.count_recorded = false;
    g_app.display_temperature_valid = false;
    g_app.status.temperature_valid = false;
    if (isfinite(pressure_mmhg) && pressure_mmhg > 0.0f && pressure_mmhg <= APP_MAX_TARGET_PRESSURE_MMHG) {
        g_app.status.settings.pressure_mmhg = pressure_mmhg;
    }

    if (mode_uses_heat(mode)) {
        g_app.status.state = APP_STATE_PREHEAT;
    } else {
        g_app.status.state = APP_STATE_READY;
    }
    set_led();
    LOGI("[App] Prepared: mode=%u pressure=%ld", (unsigned)mode,
         (long)g_app.status.settings.pressure_mmhg);
    return true;
}

bool AppController_Start(void)
{
    if (g_app.status.battery_shutdown_pending || !g_app.screen_link_healthy || !g_app.eye_link_healthy ||
        (g_app.status.state != APP_STATE_PREHEAT && g_app.status.state != APP_STATE_READY) ||
        (g_app.status.charging && !g_app.debug_mode) ||
        (!g_app.status.home_valid && !g_app.quick_resume_active) ||
        (mode_uses_pressure(g_app.status.mode) && !g_app.status.pressure_zero_valid)) {
        LOGW("[App] Start rejected: state=%u charging=%u home=%u zero=%u",
             (unsigned)g_app.status.state, g_app.status.charging ? 1U : 0U,
             g_app.status.home_valid ? 1U : 0U,
             g_app.status.pressure_zero_valid ? 1U : 0U);
        return false;
    }
    if (mode_uses_pressure(g_app.status.mode) &&
        (g_app.port == NULL || g_app.port->pressure_control_start == NULL ||
         !g_app.port->pressure_control_start(g_app.status.settings.pressure_mmhg))) {
        AppController_RaiseFault(APP_FAULT_MOTOR_COMM);
        return false;
    }
    if (!consume_eye_at_first_actuation()) {
        if (g_app.port != NULL && g_app.port->pressure_control_stop != NULL) {
            g_app.port->pressure_control_stop();
        }
        return false;
    }

    g_app.aging_running = false;
    g_app.status.state = APP_STATE_RUNNING;
    g_app.treatment_started_ms = now_ms();
    g_app.treatment_duration_ms = (uint32_t)g_app.status.settings.runtime_minutes * 60000U;
    g_app.status.remaining_ms = g_app.treatment_duration_ms;
    g_app.last_heat_ms = now_ms();
    g_app.last_pressure_ms = g_app.last_heat_ms;
    if (g_app.port != NULL && g_app.port->screen_float != NULL) {
        g_app.port->screen_float(SCREEN_TIMER_START, 0.0f);
    }
    set_led();
    LOGI("[App] Treatment started: mode=%u", (unsigned)g_app.status.mode);
    return true;
}

bool AppController_StartAging(void)
{
    if (!g_app.engineering_mode || !AppController_Start()) return false;
    g_app.aging_running = true;
    g_app.status.remaining_ms = UINT32_MAX;
    return true;
}

void AppController_Stop(AppStopReason reason)
{
    g_app.aging_running = false;
    if (g_app.status.state == APP_STATE_HOMING || g_app.status.state == APP_STATE_SHUTDOWN) {
        return;
    }
    if ((reason == APP_STOP_NATURAL && g_app.status.state != APP_STATE_RUNNING) ||
        (reason == APP_STOP_USER && !treatment_is_open())) {
        return;
    }
    safe_outputs_off();
    g_app.status.remaining_ms = 0U;
    g_app.status.pressure_valid = false;
    g_app.status.stop_reason = reason;
    g_app.over_temperature_timing = false;
    LOGI("[App] Treatment stop: reason=%u state=%u", (unsigned)reason,
         (unsigned)g_app.status.state);
    record_natural_finish_once();
    if (g_app.port != NULL && g_app.port->screen_float != NULL) {
        g_app.port->screen_float(SCREEN_TIMER_STOP, 0.0f);
        g_app.port->screen_float(SCREEN_WORK_QUIT, 0.0f);
    }
    if (reason == APP_STOP_USER) {
        request_yellow_warning_flash();
        LOGI("[LED] Manual stop yellow warning flash requested");
    }
    if (reason == APP_STOP_POWER_LOSS || reason == APP_STOP_LOW_BATTERY || g_app.status.battery_shutdown_pending) {
        g_app.pending_power_off = true;
    }
    begin_home();
    set_led();
}

void AppController_RaiseFault(AppFault fault)
{
    if (fault == APP_FAULT_CONTROL_STALE) {
        if (g_app.status.fault == fault && g_app.status.state == APP_STATE_FAULT) return;
        safe_outputs_off();
        g_app.status.fault = fault; g_app.status.state = APP_STATE_FAULT;
        g_app.status.remaining_ms = 0; g_app.status.home_valid = false;
        g_app.status.temperature_valid = g_app.status.pressure_valid = false;
        g_app.home_started = g_app.zero_pending = false;
        g_app.status.stop_reason = APP_STOP_FAULT;
        set_led();
        return; /* Panic-disabled outputs require a reset; never start homing. */
    }
    if (fault == APP_FAULT_NONE) {
        return;
    }
    g_app.status.fault = fault;
    if (fault == APP_FAULT_TMP112_COMM) {
        g_app.tmp112_recovery_confirmed = false;
    }
    LOGE("[Fault] code=0x%08lX state=%u", (unsigned long)fault,
         (unsigned)g_app.status.state);
    if (g_app.status.state != APP_STATE_HOMING && g_app.status.state != APP_STATE_SHUTDOWN) {
        /* Let the screen close its treatment UI before reporting the fault. */
        AppController_Stop(APP_STOP_FAULT);
    }
    /* If homing itself failed, begin_home() has replaced and reported the
     * more relevant motor fault already. */
    if (g_app.status.fault == fault) {
        report_fault_if_eye_online(fault);
    }
}

void AppController_SetEyeState(AppEyeState eye)
{
    AppEyeState previous = g_app.status.eye;
    bool changed = eye != previous;

    if (previous == APP_EYE_IN_USE && eye != APP_EYE_ABSENT) {
        if (g_app.port != NULL && g_app.port->screen_float != NULL) {
            g_app.port->screen_float(SCREEN_EYE_STATE, 1.0f);
        }
        return;
    }
    g_app.status.eye = eye;
    if (changed) {
        LOGI("[Eye] state=%u -> %u", (unsigned)previous, (unsigned)eye);
    }
    if (previous == APP_EYE_ABSENT && eye != APP_EYE_ABSENT) {
        g_app.tmp112_recovery_confirmed = true;
    }
    /* Keep refreshing the LCD eye state on every eye poll. The LCD protocol
     * has no acknowledgement, so a one-shot state-change frame is not enough
     * to guarantee that an unplug event is displayed. */
    if (!eye_is_screen_online(eye)) {
        clear_screen_fault_before_eye_offline();
    }
    if (g_app.port != NULL && g_app.port->screen_float != NULL) {
        g_app.port->screen_float(SCREEN_EYE_STATE,
                                eye_is_screen_online(eye) ? 1.0f : 0.0f);
        if (eye == APP_EYE_NEW && changed) {
            g_app.port->screen_float(SCREEN_NEW_EYE, 0.0f);
        }
    }
    if (eye == APP_EYE_ABSENT && treatment_is_open()) {
        /* Eye removal is a recoverable treatment interruption, not a latched
         * fault. Reuse the board's three-flash warning pattern before the
         * normal stop path returns the LEDs to idle white. */
        request_yellow_warning_flash();
        LOGW("[Eye] Removed during treatment; warning flash requested");
        AppController_Stop(APP_STOP_EYE_REMOVED);
    }
}

/* Coarse voltage indication, not a calibrated SOC/capacity estimate. */
static uint8_t battery_level_from_voltage(uint16_t mv)
{
    static const uint16_t voltage[] = {3150,3250,3500,3700,3900,4200};
    static const uint8_t level[] = {0,5,25,50,75,100};
    if (mv <= voltage[0]) return 0;
    for (unsigned i=1;i<sizeof(level);i++) {
        if (mv <= voltage[i]) return level[i-1] +
            (uint32_t)(mv-voltage[i-1])*(level[i]-level[i-1])/(voltage[i]-voltage[i-1]);
    }
    return 100;
}
/* Display-only filtering. Warning/shutdown still use each fresh raw voltage. */
static void update_battery_display(uint16_t mv, bool full)
{
    uint32_t now = now_ms();
    if (!g_app.battery_display_initialized) {
        g_app.battery_display_initialized = true;
        g_app.battery_filtered_mv = mv;
        g_app.status.battery_level = battery_level_from_voltage(mv);
        g_app.battery_display_change_ms = now;
    } else {
        uint32_t dt = now - g_app.battery_display_sample_ms;
        if (dt > 1000U) dt = 1000U; /* A read gap is not evidence of stable voltage. */
        g_app.battery_filtered_mv += (mv - g_app.battery_filtered_mv) *
                                    ((float)dt / (5000.0f + dt));
    }
    g_app.battery_display_sample_ms = now;
    if (full) {
        g_app.status.battery_level = 100;
        g_app.battery_display_direction = 0;
        g_app.battery_display_change_ms = now;
        return;
    }
    uint8_t target = battery_level_from_voltage((uint16_t)g_app.battery_filtered_mv);
    int8_t direction = target > g_app.status.battery_level ? 1 :
                       target < g_app.status.battery_level ? -1 : 0;
    if (!direction || direction != g_app.battery_display_direction) {
        g_app.battery_display_direction = direction;
        g_app.battery_display_change_ms = now;
    } else if (now - g_app.battery_display_change_ms >= 5000U) {
        g_app.status.battery_level = (uint8_t)(g_app.status.battery_level + direction);
        g_app.battery_display_change_ms = now;
    }
}

void AppController_PowerReadFailed(void) {
    g_app.battery_display_direction = 0;
    g_app.battery_display_change_ms = now_ms();
    g_app.low_voltage_samples = 0;
    g_app.warning_recovery_active = false;
}

void AppController_SetPower(bool charging, bool full, uint16_t soc, uint16_t millivolts)
{
    if (millivolts == 0U) { AppController_PowerReadFailed(); return; }
    bool changed = !g_app.power_state_logged ||
                   g_app.status.charging != charging ||
                   g_app.status.charge_full != (charging && full);

    g_app.status.charging = charging;
    g_app.status.charge_full = charging && full;
    g_app.status.battery_soc = soc;
    g_app.status.battery_mv = millivolts;
    update_battery_display(millivolts, charging && full);
    if (millivolts <= APP_LOW_BATTERY_WARNING_MV) g_app.status.low_battery_warning = true;
    /* Unloaded voltage rebound cannot clear a latched low-battery warning. */
    if (charging && millivolts >= APP_LOW_BATTERY_WARNING_CLEAR_MV &&
        g_app.status.low_battery_warning) {
        if (!g_app.warning_recovery_active) {
            g_app.warning_recovery_active = true;
            g_app.warning_recovery_started_ms = now_ms();
        } else if (now_ms() - g_app.warning_recovery_started_ms >=
                   APP_LOW_BATTERY_WARNING_RECOVERY_MS) {
            g_app.status.low_battery_warning = false;
            g_app.warning_recovery_active = false;
        }
    } else {
        g_app.warning_recovery_active = false;
    }
    if (changed) {
        LOGI("[Power] external=%u full=%u soc=%u voltage=%umV",
             charging ? 1U : 0U, g_app.status.charge_full ? 1U : 0U,
             (unsigned)soc, (unsigned)millivolts);
        g_app.power_state_logged = true;
    }

    if (charging) {
        g_app.low_voltage_samples = 0U;
        if (g_app.status.battery_shutdown_pending) g_app.pending_power_off = false;
        g_app.status.battery_shutdown_pending = false;
        if (g_app.pending_power_off &&
            (g_app.status.stop_reason == APP_STOP_POWER_LOSS || g_app.status.stop_reason == APP_STOP_LOW_BATTERY)) {
            /* External power has priority.  A PWR_SENSE edge during charger
             * insertion/termination must not disconnect the battery FET. */
            g_app.pending_power_off = false;
            LOGW("[Power] Cancelled pending shutdown: external power present");
        }
        if (treatment_is_open() && !g_app.debug_mode) {
            AppController_Stop(APP_STOP_CHARGING);
        }
    } else if (millivolts != 0U && millivolts <= APP_LOW_BATTERY_SHUTDOWN_MV) {
        if (g_app.low_voltage_samples < APP_LOW_BATTERY_CONFIRM_SAMPLES) {
            g_app.low_voltage_samples++;
        }
        if (g_app.low_voltage_samples >= APP_LOW_BATTERY_CONFIRM_SAMPLES &&
            g_app.status.state != APP_STATE_SHUTDOWN) {
            g_app.status.battery_shutdown_pending = true;
            g_app.status.battery_level = 0;
            if (g_app.status.state == APP_STATE_RUNNING && g_app.aging_running) {
                /* Continuous aging has no countdown endpoint: stop on confirmed low voltage. */
                AppController_Stop(APP_STOP_LOW_BATTERY);
            } else if (g_app.status.state == APP_STATE_RUNNING) {
                /* Keep the current full treatment; STOP will request power-off. */
            } else if (g_app.status.state == APP_STATE_HOMING) {
                g_app.status.stop_reason = APP_STOP_LOW_BATTERY;
                g_app.pending_power_off = true;
            } else {
                AppController_Stop(APP_STOP_LOW_BATTERY);
            }
        }
    } else {
        g_app.low_voltage_samples = 0U;
    }
    if (g_app.status.battery_shutdown_pending) g_app.status.battery_level = 0;
    else if (g_app.status.low_battery_warning && g_app.status.battery_level > 5)
        g_app.status.battery_level = 5;
    send_float(SCREEN_SOC, (float)g_app.status.battery_level);
    if (eye_is_screen_online(g_app.status.eye) &&
        g_app.port != NULL && g_app.port->screen_u32 != NULL) {
        /* New screens treat this as a renewable fault status. Old screens do
         * not know 0x2F01 and safely ignore it. A zero value clears the popup.
         * Do not transmit this frame while the eye shield is offline; its
         * 0x2055 offline state and a fault status must remain mutually exclusive. */
        g_app.port->screen_u32(SCREEN_FAULT_STATUS, (uint32_t)g_app.status.fault);
    }
    set_led();
}

void AppController_SetEyeLinkHealthy(bool healthy)
{
    bool restart_pressure;

    if (healthy == g_app.eye_link_healthy) {
        return;
    }
    g_app.eye_link_healthy = healthy;
    if (!healthy) {
        g_app.temp_comm_failure_timing = false;
        if (treatment_is_open()) {
            g_app.eye_link_paused_pressure =
                g_app.status.state == APP_STATE_RUNNING &&
                mode_uses_pressure(g_app.status.mode);
            safe_outputs_off();
            LOGW("[Eye] I2C link interrupted; outputs paused during reconnect window");
        }
        return;
    }

    restart_pressure = g_app.eye_link_paused_pressure &&
                       g_app.status.state == APP_STATE_RUNNING &&
                       mode_uses_pressure(g_app.status.mode);
    g_app.eye_link_paused_pressure = false;
    g_app.temp_comm_failure_timing = false;
    if (restart_pressure &&
        (g_app.port == NULL || g_app.port->pressure_control_start == NULL ||
         !g_app.port->pressure_control_start(g_app.status.settings.pressure_mmhg))) {
        LOGE("[Eye] Pressure restart failed after I2C reconnect");
        AppController_RaiseFault(APP_FAULT_MOTOR_COMM);
        return;
    }
    LOGI("[Eye] I2C link recovered inside reconnect window");
}

void AppController_SetScreenLinkHealthy(bool healthy)
{
    g_app.screen_link_healthy = healthy;
    if (!healthy && treatment_is_open()) AppController_RaiseFault(APP_FAULT_SCREEN_TIMEOUT);
    if (healthy && g_app.status.fault == APP_FAULT_SCREEN_TIMEOUT &&
        g_app.status.state == APP_STATE_FAULT && g_app.status.home_valid) {
        g_app.status.fault = APP_FAULT_NONE;
        g_app.status.state = APP_STATE_IDLE;
        set_led();
    }
}

void AppController_NotifyPowerLoss(void)
{
    if (g_app.status.charging) {
        LOGW("[Power] Ignored PWR_SENSE edge while external power is present");
        return;
    }
    if (g_app.status.state == APP_STATE_HOMING) {
        g_app.status.stop_reason = APP_STOP_POWER_LOSS;
        g_app.pending_power_off = true;
    } else if (g_app.status.state != APP_STATE_SHUTDOWN) {
        AppController_Stop(APP_STOP_POWER_LOSS);
    }
}

void AppController_SetTemperature(float temperature_c)
{
    if (isfinite(temperature_c) && temperature_c > 0.0f && temperature_c <= APP_MAX_TARGET_TEMPERATURE_C) {
        g_app.status.settings.temperature_c = temperature_c;
    }
}

void AppController_SetPressure(float pressure_mmhg)
{
    if (isfinite(pressure_mmhg) && pressure_mmhg > 0.0f && pressure_mmhg <= APP_MAX_TARGET_PRESSURE_MMHG) {
        g_app.status.settings.pressure_mmhg = pressure_mmhg;
    }
}

void AppController_SetRuntime(uint16_t minutes)
{
    if (minutes > 0U && minutes <= APP_MAX_RUNTIME_MINUTES && g_app.status.state != APP_STATE_RUNNING) {
        g_app.status.settings.runtime_minutes = minutes;
        g_app.status.remaining_ms = (uint32_t)minutes * 60000U;
    }
}

void AppController_SetDebugMode(bool enabled)
{
    if (g_app.debug_mode == enabled) {
        return;
    }
    g_app.debug_mode = enabled;
    LOGW("[Debug] Test mode %s; charging treatment interlock %s",
         enabled ? "enabled" : "disabled",
         enabled ? "bypassed" : "active");
    if (!enabled && treatment_is_open()) {
        AppController_Stop(g_app.status.charging ? APP_STOP_CHARGING : APP_STOP_USER);
    }
}

bool AppController_DebugMode(void)
{
    return g_app.debug_mode;
}

bool AppController_SavePreset(uint8_t preset, uint16_t pressure, uint16_t minutes)
{
    if (g_app.status.state != APP_STATE_IDLE || !g_app.port || !g_app.port->save_preset) return false;
    if (!g_app.port->save_preset(preset, pressure, minutes)) return false;
    if (g_app.port->screen_boot_sync) g_app.port->screen_boot_sync();
    return true;
}

void AppController_ScreenBoot(void)
{
    if (g_app.port != NULL && g_app.port->screen_boot_sync != NULL) {
        g_app.port->screen_boot_sync();
    }
}

uint16_t AppController_StorageRead(uint8_t address, uint16_t default_value)
{
    return g_app.port != NULL && g_app.port->storage_read_u16 != NULL ?
           g_app.port->storage_read_u16(address, default_value) : default_value;
}

bool AppController_StorageWrite(uint8_t address, uint16_t value)
{
    return !treatment_is_open() && g_app.status.state == APP_STATE_IDLE &&
           g_app.port != NULL && g_app.port->storage_write_u16 != NULL &&
           g_app.port->storage_write_u16(address, value);
}

void AppController_SetEngineeringMode(bool enabled) {
    if(g_app.engineering_mode && !enabled && !PressureTuning_Leased()) PressureTuning_ResetDefaults();
    g_app.engineering_mode = enabled;
}
bool AppController_EngineeringMode(void) { return g_app.engineering_mode; }

bool AppController_StorageEraseMain(void)
{
    if ((g_app.debug_mode || g_app.engineering_mode) && g_app.status.state == APP_STATE_IDLE && g_app.port != NULL && g_app.port->storage_erase_main != NULL) {
        return g_app.port->storage_erase_main();
    }
    return false;
}

bool AppController_StorageEraseEye(void)
{
    if ((g_app.debug_mode || g_app.engineering_mode) && g_app.status.state == APP_STATE_IDLE && g_app.port != NULL && g_app.port->storage_erase_eye != NULL) {
        return g_app.port->storage_erase_eye();
    }
    return false;
}

static void tick_homing(void)
{
    AppAsyncResult result;
    AppFault zero_fault;
    bool begin_zero = false;

    if ((!g_app.home_started && !g_app.zero_pending) || g_app.port == NULL || g_app.port->home_poll == NULL) {
        return;
    }
    if (!g_app.zero_pending) {
    result = g_app.port->home_poll();
    if (result == APP_ASYNC_BUSY) {
        return;
    }
    g_app.home_started = false;
    if (result == APP_ASYNC_FAILED || result == APP_ASYNC_COMM_FAILED) {
        AppFault fault = result == APP_ASYNC_COMM_FAILED ?
                         APP_FAULT_MOTOR_COMM : APP_FAULT_MOTOR_HOME;
        LOGE("[Motor] Homing failed fault=0x%04X", (unsigned)fault);
        g_app.status.fault = fault;
        report_fault_if_eye_online(fault);
        if (g_app.port->screen_float != NULL) {
            g_app.port->screen_float(SCREEN_HOME_COMPLETE, 0.0f);
        }
        if (g_app.pending_power_off && g_app.port->power_latch_off != NULL) {
            g_app.port->power_latch_off();
        }
        g_app.status.state = APP_STATE_FAULT;
        set_led();
        return;
    }

    g_app.status.home_valid = true;
    begin_zero = true;
    g_app.zero_pending = true;
    }
    zero_fault = APP_FAULT_PRESSURE_COMM;
    result = g_app.port->pressure_zero_poll != NULL ?
             g_app.port->pressure_zero_poll(begin_zero, &zero_fault) : APP_ASYNC_FAILED;
    if (result == APP_ASYNC_BUSY) return;
    g_app.zero_pending = false;
    g_app.status.pressure_zero_valid = zero_fault == APP_FAULT_NONE;
    if (zero_fault != APP_FAULT_NONE) {
        g_app.status.fault = zero_fault;
        report_fault_if_eye_online(zero_fault);
    }
    LOGI("[Motor] Homing complete, pressure_zero=%u",
         g_app.status.pressure_zero_valid ? 1U : 0U);
    if (g_app.port->screen_float != NULL) {
        g_app.port->screen_float(SCREEN_HOME_COMPLETE, 1.0f);
    }
    if (g_app.pending_power_off) {
        g_app.status.state = APP_STATE_SHUTDOWN;
        if (g_app.port->power_latch_off != NULL) {
            g_app.port->power_latch_off();
        }
        return;
    }
    if (g_app.status.fault == APP_FAULT_OVER_PRESSURE &&
        g_app.status.pressure_zero_valid) {
        LOGW("[Fault] Overpressure latch cleared after successful home and zero calibration");
        g_app.status.fault = APP_FAULT_NONE;
    } else if (g_app.status.fault == APP_FAULT_OVER_TEMPERATURE) {
        LOGW("[Fault] Overtemperature latch cleared after successful home");
        g_app.status.fault = APP_FAULT_NONE;
    }
    g_app.status.mode = APP_MODE_NONE;
    g_app.status.state = g_app.status.fault == APP_FAULT_NONE ? APP_STATE_IDLE : APP_STATE_FAULT;
    set_led();
}

static void tick_heat(uint32_t now)
{
    float measured = 0.0f;
    float target;
    float control_target;
    float screen_temperature;

    if (!mode_uses_heat(g_app.status.mode) ||
        (g_app.status.state != APP_STATE_PREHEAT && g_app.status.state != APP_STATE_RUNNING) ||
        now - g_app.last_heat_ms < APP_HEAT_CONTROL_PERIOD_MS) {
        return;
    }
    g_app.last_heat_ms = now;
    target = g_app.status.state == APP_STATE_PREHEAT ? APP_PREHEAT_TARGET_C :
                                                     g_app.status.settings.temperature_c;
    control_target = target + PRODUCT_TEMPERATURE_CONTROL_COMPENSATION_C;
    if (g_app.port == NULL || g_app.port->heater_control == NULL ||
        !g_app.port->heater_control(control_target, &measured)) {
        g_app.status.temperature_valid = false;
        if (!eye_is_screen_online(g_app.status.eye)) {
            g_app.temp_comm_failure_timing = false;
            return;
        }
        if (!g_app.temp_comm_failure_timing) {
            g_app.temp_comm_failure_timing = true;
            g_app.temp_comm_failure_started_ms = now;
            LOGW("[Temp] Read failed; heating inhibited during %lums confirmation window",
                 (unsigned long)APP_TEMP_COMM_FAULT_CONFIRM_MS);
        } else if (now - g_app.temp_comm_failure_started_ms >=
                   APP_TEMP_COMM_FAULT_CONFIRM_MS) {
            g_app.temp_comm_failure_timing = false;
            safe_outputs_off();
            AppController_RaiseFault(APP_FAULT_TMP112_COMM);
        }
        return;
    }
    if (g_app.temp_comm_failure_timing) {
        LOGI("[Temp] Communication recovered; confirmation cancelled");
    }
    g_app.temp_comm_failure_timing = false;
    g_app.status.measured_temperature_c = measured;
    g_app.status.temperature_valid = true;
    if (measured >= APP_MAX_SAFE_TEMPERATURE_C) {
        if (!g_app.over_temperature_timing) {
            g_app.over_temperature_timing = true;
            g_app.over_temperature_started_ms = now;
            LOGW("[Temp] Overtemperature timing started measured_x10=%ld threshold_x10=%ld",
                 (long)(measured * 10.0f),
                 (long)(APP_MAX_SAFE_TEMPERATURE_C * 10.0f));
        } else if (now - g_app.over_temperature_started_ms >=
                   APP_OVER_TEMPERATURE_CONFIRM_MS) {
            LOGE("[Temp] Overtemperature confirmed for %lu ms measured_x10=%ld",
                 (unsigned long)(now - g_app.over_temperature_started_ms),
                 (long)(measured * 10.0f));
            g_app.over_temperature_timing = false;
            AppController_RaiseFault(APP_FAULT_OVER_TEMPERATURE);
            return;
        }
    } else if (g_app.over_temperature_timing) {
        LOGI("[Temp] Overtemperature timing cancelled measured_x10=%ld",
             (long)(measured * 10.0f));
        g_app.over_temperature_timing = false;
    }
    /* Smooth only the value shown to the user. Safety and heater control keep
     * using the unfiltered sensor value above. */
    if (!g_app.display_temperature_valid) {
        g_app.display_temperature_c = measured;
        g_app.display_temperature_valid = true;
    } else {
        g_app.display_temperature_c += APP_TEMP_DISPLAY_FILTER_ALPHA *
                                       (measured - g_app.display_temperature_c);
    }
    g_app.status.display_temperature_c = fminf(g_app.display_temperature_c - PRODUCT_TEMPERATURE_CONTROL_COMPENSATION_C, APP_MAX_DISPLAY_TEMPERATURE_C);
    if (now - g_app.last_temp_display_ms >= APP_TEMP_DISPLAY_PERIOD_MS) {
        screen_temperature = g_app.display_temperature_c -
                             PRODUCT_TEMPERATURE_CONTROL_COMPENSATION_C;
        if (screen_temperature > APP_MAX_DISPLAY_TEMPERATURE_C) {
            screen_temperature = APP_MAX_DISPLAY_TEMPERATURE_C;
        }
        send_float(g_app.status.mode == APP_MODE_AUTO ? SCREEN_TEMP_AUTO : SCREEN_TEMP_HEAT,
                   screen_temperature);
        g_app.last_temp_display_ms = now;
    }
    if (now - g_app.last_temp_log_ms >= APP_SENSOR_LOG_PERIOD_MS) {
        int32_t measured_x10 = (int32_t)(measured * 10.0f);
        int32_t target_x10 = (int32_t)(target * 10.0f);
        int32_t control_target_x10 = (int32_t)(control_target * 10.0f);
        LOGI("[Temp] measured=%ld.%01ldC target=%ld.%01ldC control_target=%ld.%01ldC",
             (long)(measured_x10 / 10), (long)(measured_x10 % 10),
             (long)(target_x10 / 10), (long)(target_x10 % 10),
             (long)(control_target_x10 / 10),
             (long)(control_target_x10 % 10));
        g_app.last_temp_log_ms = now;
    }
}

static void tick_pressure(uint32_t now)
{
    float measured = 0.0f;
    AppFault fault;

    if (!mode_uses_pressure(g_app.status.mode) || g_app.status.state != APP_STATE_RUNNING ||
        now - g_app.last_pressure_ms < APP_PRESSURE_CONTROL_PERIOD_MS) {
        return;
    }
    g_app.last_pressure_ms = now;
    if (g_app.port == NULL || g_app.port->pressure_control_step == NULL) {
        AppController_RaiseFault(APP_FAULT_PRESSURE_COMM);
        return;
    }
    fault = g_app.port->pressure_control_step(g_app.status.settings.pressure_mmhg,
                                              &measured);
    if (fault != APP_FAULT_NONE) {
        g_app.status.pressure_valid = false;
        PressureLimit_Reset(&g_app.pressure_limit);
        AppController_RaiseFault(fault);
        return;
    }
    g_app.status.measured_pressure_mmhg = measured;
    g_app.status.pressure_valid = isfinite(measured) &&
        (!g_app.port->pressure_sample_valid || g_app.port->pressure_sample_valid());
    if (PressureLimit_Update(&g_app.pressure_limit, measured, now_ms())) {
        /* Stop the motor before formatting or transmitting fault diagnostics. */
        if (g_app.port != NULL && g_app.port->pressure_control_stop != NULL) {
            g_app.port->pressure_control_stop();
        }
        AppController_RaiseFault(APP_FAULT_OVER_PRESSURE);
        return;
    }
    if (now - g_app.last_pressure_display_ms >= APP_PRESSURE_DISPLAY_PERIOD_MS) {
        send_float(g_app.status.mode == APP_MODE_AUTO ? SCREEN_PRESSURE_AUTO : SCREEN_PRESSURE, measured);
        g_app.last_pressure_display_ms = now;
    }
}

void AppController_Tick(void)
{
    uint32_t now = now_ms();

    if (g_app.status.state == APP_STATE_RUNNING && !g_app.aging_running) {
        uint32_t elapsed = now - g_app.treatment_started_ms;
        if (elapsed >= g_app.treatment_duration_ms) {
            AppController_Stop(APP_STOP_NATURAL);
            return;
        }
        g_app.status.remaining_ms = g_app.treatment_duration_ms - elapsed;
    }
    try_clear_tmp112_fault();
    if (!g_app.eye_link_healthy && treatment_is_open()) {
        return;
    }
    if (g_app.status.state == APP_STATE_HOMING) {
        tick_homing();
        return;
    }
    tick_heat(now);
    /* A transient temperature read failure retains output state. Continue
     * pressure feedback and its protections during the confirmation window. */
    tick_pressure(now);
}

const AppSnapshot *AppController_Status(void)
{
    return &g_app.status;
}
