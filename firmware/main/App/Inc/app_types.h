#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "system_state.h"

typedef struct {
    float temperature_c;
    float pressure_mmhg;
    uint16_t runtime_minutes;
} AppTreatmentSettings;

typedef struct {
    AppState state;
    AppMode mode;
    AppEyeState eye;
    AppStopReason stop_reason;
    AppFault fault;
    AppTreatmentSettings settings;
    float measured_temperature_c;
    float display_temperature_c;
    bool temperature_valid;
    bool charging;
    bool charge_full;
    bool low_battery_warning;
    bool battery_shutdown_pending;
    uint8_t battery_level;
    bool home_valid;
    bool pressure_zero_valid;
    uint16_t battery_soc;
    uint16_t battery_mv;
    uint32_t session_id;
    uint32_t remaining_ms;
    float measured_pressure_mmhg;
    bool pressure_valid;
} AppSnapshot;

#endif
