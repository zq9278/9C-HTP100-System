#include "pressure_tuning.h"

#include <math.h>
#include <string.h>

#include "app_log.h"

/* Production uses compiled profiles; Tuning permits temporary, leased RTT overrides. */
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

void PressureTuning_ResetDefaults(void)
{
    memcpy(g_profiles, g_defaults, sizeof(g_profiles));
}

void PressureTuning_Init(void)
{
    PressureTuning_ResetDefaults();
    if (TUNING_USE_CODE_DEFAULTS != 0U) {
        LOGW("[Tuning] Production compiled profiles; temporary overrides disabled");
        return;
    }

}

const PressureTuningProfile *PressureTuning_ProfileForTarget(float target_mmhg)
{
    const PressureTuningProfile *profiles =
        TUNING_USE_CODE_DEFAULTS != 0U ? g_defaults : g_profiles;
    if (target_mmhg <= 200.0f) return &profiles[0];
    if (target_mmhg <= 300.0f) return &profiles[1];
    if (target_mmhg <= 400.0f) return &profiles[2];
    if (target_mmhg <= 500.0f) return &profiles[3];
    return &profiles[4];
}

bool PressureTuning_Get(uint8_t index, PressureTuningProfile *profile)
{
    if (index >= PRESSURE_TUNING_PROFILE_COUNT || profile == NULL) {
        return false;
    }
    *profile = TUNING_USE_CODE_DEFAULTS != 0U ? g_defaults[index] : g_profiles[index];
    return true;
}

bool PressureTuning_Set(uint8_t index, const PressureTuningProfile *profile)
{
    if (TUNING_USE_CODE_DEFAULTS != 0U) {
        LOGW("[Tuning] Host profile update rejected: CODE TABLE LOCKED");
        return false;
    }
    if (index >= PRESSURE_TUNING_PROFILE_COUNT || !profile_valid(profile)) {
        return false;
    }
    g_profiles[index] = *profile;
    return true;
}

bool PressureTuning_Save(void)
{
    /* Live tuning is temporary; production always uses the compiled table. */
    return false;
}
