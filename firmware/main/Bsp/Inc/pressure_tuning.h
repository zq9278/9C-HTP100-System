#ifndef PRESSURE_TUNING_H
#define PRESSURE_TUNING_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define PRESSURE_TUNING_PROFILE_COUNT 5U

typedef struct {
    float fast_speed; /* 前进速度上限，驱动器速度单位 */
    float retract_speed; /* 回退及反向补偿速度上限 */
    float kp; /* 压差到速度的比例增益 */
    float approach_ratio; /* 减速区间占目标压力的比例，0.01～1 */
} PressureTuningProfile;

typedef bool (*PressureRead)(uint8_t, uint8_t *, size_t);
typedef bool (*PressureWrite)(uint8_t, const uint8_t *, size_t);
void PressureTuning_Init(void);
bool PressureTuning_Load(PressureRead read, PressureWrite write);
bool PressureTuning_Apply(uint8_t index, const PressureTuningProfile *profile);
bool PressureTuning_SaveProfile(uint8_t index, const PressureTuningProfile *profile);
bool PressureTuning_Persisted(void);
void PressureTuning_SetLease(bool owned);
bool PressureTuning_Leased(void);
const PressureTuningProfile *PressureTuning_ProfileForTarget(float target_mmhg);
bool PressureTuning_Get(uint8_t index, PressureTuningProfile *profile);
bool PressureTuning_Set(uint8_t index, const PressureTuningProfile *profile);
void PressureTuning_ResetDefaults(void);
bool PressureTuning_Save(void);

#endif
