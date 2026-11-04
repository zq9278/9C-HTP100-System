#include "system_link.h"
#include "system_wire.h"
#include "app_controller.h"
#include "app_config.h"
#include "board_port.h"
#include "main.h"
#include <math.h>
#include <string.h>

static SwParser parser;
static uint32_t boot_id, peer_id, last_heartbeat, last_snapshot;
static uint16_t snapshot_sequence, request_sequence, acknowledged_sequence;
static bool peer_seen, request_seen, snapshot_pending;
static uint8_t last_request[SW_REQUEST_BYTES], last_result[8];
volatile uint32_t g_link_tx_failures, g_link_rejected_requests;

static bool healthy(void) { return peer_seen && HAL_GetTick() - last_heartbeat < SW_TIMEOUT_MS; }
static void send(uint8_t type, uint16_t sequence, const uint8_t *payload, uint8_t size)
{
    uint8_t frame[SW_MAX_FRAME];
    size_t n = Sw_Encode(frame, type, sequence, payload, size);
    if (!Board_ScreenWrite(frame, (uint16_t)n)) g_link_tx_failures++;
}
static uint8_t execute(uint16_t cmd, float value, uint16_t duration_s)
{
    const AppSnapshot *s = AppController_Status();
    bool prepare = cmd == 0x1041 || cmd == 0x1005 || cmd == 0x1037;
    if (!healthy()) return SW_NO_LINK;
    if (!isfinite(value)) return SW_BAD_VALUE;
    if (cmd >= 0x9101 && cmd <= 0x9103) {
        if (value <= 0 || value > 650 || value != floorf(value) || !duration_s || duration_s > 3600 || duration_s % 60) return SW_BAD_VALUE;
        return AppController_SavePreset((uint8_t)(cmd-0x9100), (uint16_t)value, duration_s/60U) ? SW_OK : SW_REJECTED;
    }
    if (cmd == 0x9200) {
        if (value != 0 && value != 1) return SW_BAD_VALUE;
        if (value && s->state != APP_STATE_IDLE) return SW_BUSY;
        if (!value && AppController_EngineeringMode() &&
            (s->state == APP_STATE_RUNNING || s->state == APP_STATE_PREHEAT || s->state == APP_STATE_READY))
            AppController_Stop(APP_STOP_USER);
        AppController_SetEngineeringMode(value != 0); return SW_OK;
    }
    if (cmd == 0x9201) {
        if (!AppController_EngineeringMode()) return SW_REJECTED;
        if (s->state != APP_STATE_IDLE) return SW_BUSY;
        if (value <= 0 || value > APP_MAX_TARGET_PRESSURE_MMHG ||
            duration_s > APP_MAX_RUNTIME_MINUTES * 60U || duration_s % 60U) return SW_BAD_VALUE;
        if (!AppController_Prepare(APP_MODE_AUTO, value)) return SW_REJECTED;
        AppController_SetTemperature(42.5f);
        if (AppController_StartAging()) return SW_OK;
        AppController_Stop(APP_STOP_USER); return SW_REJECTED;
    }
    if (cmd == 0x9202 || cmd == 0x9203) {
        if (!AppController_EngineeringMode()) return SW_REJECTED;
        if (s->state != APP_STATE_IDLE) return SW_BUSY;
        return (cmd == 0x9202 ? AppController_StorageEraseMain() : AppController_StorageEraseEye()) ? SW_OK : SW_REJECTED;
    }
    if (prepare) {
        AppMode mode = cmd == 0x1041 ? APP_MODE_HEAT : cmd == 0x1005 ? APP_MODE_PRESSURE : APP_MODE_AUTO;
        if (!duration_s || duration_s > APP_MAX_RUNTIME_MINUTES * 60U || duration_s % 60U) return SW_BAD_VALUE;
        if (mode != APP_MODE_HEAT && (value <= 0 || value > APP_MAX_TARGET_PRESSURE_MMHG)) return SW_BAD_VALUE;
        /* Only normal completed sessions with a reusable zero may interrupt home. */
        if (s->state == APP_STATE_RUNNING || s->state == APP_STATE_STOPPING || (s->state == APP_STATE_HOMING && !AppController_CanQuickResume())) return SW_BUSY;
        if (!AppController_Prepare(mode, value)) return SW_REJECTED;
        AppController_SetRuntime((uint16_t)(duration_s / 60U));
        return SW_OK;
    }
    switch (cmd) {
    case 0x1040: case 0x1006: case 0x1036:
        if (s->mode != (cmd == 0x1040 ? APP_MODE_HEAT : cmd == 0x1006 ? APP_MODE_PRESSURE : APP_MODE_AUTO)) return SW_REJECTED;
        return AppController_Start() ? SW_OK : SW_REJECTED;
    case 0x1030: case 0x1034: case 0x1038:
        AppController_Stop(APP_STOP_USER); return SW_OK;
    default: return SW_REJECTED;
    }
}
static void receive(uint8_t type, uint16_t seq, const uint8_t *p, uint8_t size)
{
    if (type == SW_HEARTBEAT && size == 15) {
        uint32_t peer = Sw_Get32(p);
        if (Sw_Get32(p + 4) != boot_id || !peer) { snapshot_pending = true; return; }
        if (peer_seen && peer_id != peer) {
            /* A restarted UI cannot inherit an unacknowledged treatment. */
            AppController_SetScreenLinkHealthy(false);
            AppController_SetEngineeringMode(false);
            request_seen = false;
        }
        uint16_t ack = Sw_Get16(p + 8);
        if (Sw_IsNewer(ack, snapshot_sequence)) return;
        bool progressed = !peer_seen || peer_id != peer || Sw_IsNewer(ack, acknowledged_sequence);
        peer_id = peer; peer_seen = true;
        if (progressed) { acknowledged_sequence = ack; last_heartbeat = HAL_GetTick(); }
        AppController_SetScreenLinkHealthy(healthy());
        if (progressed && p[10] == 1 && Sw_Get32(p + 11) == AppController_Status()->session_id) {
            AppController_Stop(APP_STOP_USER); snapshot_pending = true;
        }
        return;
    }
    if (type != SW_REQUEST || size != SW_REQUEST_BYTES) return;
    uint16_t cmd = Sw_Get16(p + 8);
    uint8_t result[8] = {0};
    Sw_Put16(result, cmd);
    if (request_seen && seq == request_sequence) {
        if (!memcmp(p, last_request, size)) send(SW_RESULT, seq, last_result, sizeof(last_result));
        else { result[2] = SW_STALE; send(SW_RESULT, seq, result, sizeof(result)); }
        snapshot_pending = true; return;
    }
    if (Sw_Get32(p) != boot_id || (request_seen && !Sw_IsNewer(seq, request_sequence)) ||
        Sw_Get32(p + 4) != AppController_Status()->session_id) {
        result[2] = SW_STALE;
    } else {
        float value; memcpy(&value, p + 10, sizeof(value));
        result[2] = execute(cmd, value, Sw_Get16(p + 14));
    }
    result[3] = (uint8_t)AppController_Status()->state;
    Sw_Put32(result + 4, AppController_Status()->session_id);
    if (!request_seen || Sw_IsNewer(seq, request_sequence)) {
        request_seen = true; request_sequence = seq;
        memcpy(last_request, p, size); memcpy(last_result, result, sizeof(result));
    }
    if (result[2] != SW_OK) g_link_rejected_requests++;
    send(SW_RESULT, seq, result, sizeof(result)); snapshot_pending = true;
}
void SystemLink_Init(void)
{
    memset(&parser, 0, sizeof(parser));
    boot_id = Sw_BootId(HAL_GetUIDw0() ^ HAL_GetUIDw1());
    peer_seen = request_seen = false; snapshot_pending = true;
    acknowledged_sequence = snapshot_sequence = request_sequence = 0;
    last_heartbeat = last_snapshot = 0;
    AppController_SetScreenLinkHealthy(false);
    AppController_SetEngineeringMode(false);
}
void SystemLink_Feed(const uint8_t *data, size_t length) { Sw_Feed(&parser, data, length, receive); }
void SystemLink_Tick(void)
{
    const AppSnapshot *s = AppController_Status();
    uint32_t now = HAL_GetTick();
    AppController_SetScreenLinkHealthy(healthy());
    if (!healthy()) AppController_SetEngineeringMode(false);
    static AppState previous_state = APP_STATE_BOOT;
    if (s->state != previous_state) { previous_state = s->state; snapshot_pending = true; }
    if (!snapshot_pending && now - last_snapshot < SW_PERIOD_MS) return;
    snapshot_pending = false; last_snapshot = now;
    uint8_t p[SW_SNAPSHOT_BYTES] = {0};
    Sw_Put32(p, boot_id); Sw_Put32(p + 4, s->session_id); Sw_Put32(p + 8, now);
    p[12] = (uint8_t)s->state; p[13] = (uint8_t)s->mode; p[14] = (uint8_t)s->eye;
    p[15] = (s->temperature_valid ? 1 : 0) | (s->pressure_valid ? 2 : 0) |
            (s->home_valid ? 4 : 0) | (s->charging ? 8 : 0) |
            (AppController_CanQuickResume() ? 16 : 0) |
            (s->low_battery_warning ? 32 : 0) | (s->battery_shutdown_pending ? 64 : 0) | (AppController_EngineeringMode() ? 128 : 0);
    Sw_Put16(p + 16, (uint16_t)s->fault); p[18] = (uint8_t)s->stop_reason; p[19] = healthy();
    Sw_Put32(p + 20, s->remaining_ms);
    Sw_Put16(p + 24, (uint16_t)(int16_t)(s->temperature_valid ? s->measured_temperature_c * 100 : 0));
    float pressure = s->pressure_valid ? fminf(fmaxf(s->measured_pressure_mmhg, 0), 6553.5f) : 0;
    Sw_Put16(p + 26, (uint16_t)(pressure * 10));
    Sw_Put16(p + 28, (uint16_t)(s->settings.temperature_c * 100));
    Sw_Put16(p + 30, (uint16_t)(s->settings.pressure_mmhg * 10));
    Sw_Put16(p + 32, s->settings.runtime_minutes * 60U); p[34] = s->battery_level;
    Sw_Put16(p + 36, s->battery_mv);
    Sw_Put16(p + 38, (uint16_t)(int16_t)(s->temperature_valid ? s->display_temperature_c * 100 : 0));
    send(SW_SNAPSHOT, ++snapshot_sequence, p, sizeof(p));
}
