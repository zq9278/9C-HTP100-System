#include "screen_link.h"
#include "system_wire.h"
#include "system_state.h"
#include "usart.h"
#include <string.h>

static SwParser parser;
static uint32_t boot_id, host_id, session_id, last_snapshot, last_heartbeat, sent_at;
static uint16_t snapshot_sequence, request_sequence;
static bool seen, pending, dirty, stop_latched;
static uint32_t stop_session;
static uint8_t attempts, result;
static uint8_t snapshot[SW_SNAPSHOT_BYTES], request[SW_REQUEST_BYTES];
volatile uint32_t g_screen_link_tx_errors, g_screen_link_request_timeouts;
static bool send(uint8_t type, uint16_t sequence, const uint8_t *p, uint8_t size)
{
    uint8_t frame[SW_MAX_FRAME];
    size_t n = Sw_Encode(frame, type, sequence, p, size);
    if (HAL_UART_Transmit(&huart1, frame, (uint16_t)n, 10) == HAL_OK) return true;
    g_screen_link_tx_errors++; return false;
}
bool ScreenLink_Healthy(void) { return seen && HAL_GetTick() - last_snapshot < SW_TIMEOUT_MS; }
bool ScreenLink_Pending(void) { return pending; }
bool ScreenLink_IsTreatmentCommand(uint16_t cmd)
{
    return cmd == 0x1041 || cmd == 0x1005 || cmd == 0x1037 || cmd == 0x1040 ||
           cmd == 0x1006 || cmd == 0x1036 || cmd == 0x1030 || cmd == 0x1034 ||
           cmd == 0x1038 || cmd == 0x8900;
}
bool ScreenLink_Request(uint16_t cmd, float value, uint16_t duration_s)
{
    if (!ScreenLink_Healthy()) { result = SW_NO_LINK; dirty = true; return false; }
    bool stop = cmd == 0x1030 || cmd == 0x1034 || cmd == 0x1038;
    if (pending && !stop) return false;
    /* A stop supersedes an in-flight start, and retries never re-send that start. */
    if (pending && stop) {
        /* Prepare may have created a newer session before its snapshot arrives.
         * Defer STOP until that state is known instead of addressing an old session. */
        uint16_t old = Sw_Get16(request + 8);
        if (old == 0x1041 || old == 0x1005 || old == 0x1037) return false;
    }
    if (cmd == 0x8900) return false; /* Countdown is owned by the controller. */
    Sw_Put32(request, host_id); Sw_Put32(request + 4, session_id);
    Sw_Put16(request + 8, cmd); memcpy(request + 10, &value, 4); Sw_Put16(request + 14, duration_s);
    if (stop) { stop_latched = true; stop_session = session_id; }
    request_sequence++; pending = true; attempts = 1; result = SW_OK;
    sent_at = HAL_GetTick(); dirty = true;
    (void)send(SW_REQUEST, request_sequence, request, sizeof(request));
    return true;
}
static void receive(uint8_t type, uint16_t sequence, const uint8_t *p, uint8_t size)
{
    if (type == SW_SNAPSHOT && size == SW_SNAPSHOT_BYTES) {
        uint32_t host = Sw_Get32(p);
        if (!host || p[12] > APP_STATE_SHUTDOWN || p[13] > APP_MODE_AUTO) return;
        if (seen && host == host_id && !Sw_IsNewer(sequence, snapshot_sequence)) return;
        if (host != host_id) { pending = false; stop_latched = false; result = SW_OK; }
        host_id = host; session_id = Sw_Get32(p + 4); snapshot_sequence = sequence;
        if (p[12] != APP_STATE_RUNNING && p[12] != APP_STATE_READY && p[12] != APP_STATE_PREHEAT) stop_latched = false;
        memcpy(snapshot, p, size); seen = true; last_snapshot = HAL_GetTick(); dirty = true;
    } else if (type == SW_RESULT && size == 8 && pending && sequence == request_sequence &&
               Sw_Get16(p) == Sw_Get16(request + 8)) {
        result = p[2]; pending = false; dirty = true;
        if (result == SW_OK && Sw_Get16(p) >= 0x9101 && Sw_Get16(p) <= 0x9103) screen_config_saved();
    }
}
void ScreenLink_Init(void)
{
    boot_id = Sw_BootId(HAL_GetUIDw0() ^ HAL_GetUIDw1());
    memset(&parser, 0, sizeof(parser));
    seen = pending = dirty = stop_latched = false; host_id = session_id = 0;
    snapshot_sequence = request_sequence = 0; result = 0;
    last_heartbeat = HAL_GetTick() - SW_PERIOD_MS;
}
void ScreenLink_Feed(const uint8_t *data, size_t size) { Sw_Feed(&parser, data, size, receive); }
void ScreenLink_Tick(void)
{
    uint32_t now = HAL_GetTick();
    if (now - last_heartbeat >= SW_PERIOD_MS) {
        uint8_t p[15]; Sw_Put32(p, boot_id); Sw_Put32(p + 4, host_id); Sw_Put16(p + 8, snapshot_sequence);
        p[10] = stop_latched; Sw_Put32(p + 11, stop_session);
        (void)send(SW_HEARTBEAT, 0, p, sizeof(p)); last_heartbeat = now;
    }
    if (pending && now - sent_at >= 250U) {
        if (attempts >= 4 || !ScreenLink_Healthy()) {
            pending = false; result = SW_NO_LINK; dirty = true; g_screen_link_request_timeouts++;
        } else {
            attempts++; sent_at = now;
            (void)send(SW_REQUEST, request_sequence, request, sizeof(request));
        }
    }
    if (!ScreenLink_Healthy()) { screen_system_link_lost(); return; }
    if (dirty) { dirty = false; screen_apply_system_state(snapshot, pending, result); }
}
