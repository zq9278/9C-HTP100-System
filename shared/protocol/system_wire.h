#ifndef SYSTEM_WIRE_H
#define SYSTEM_WIRE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SW_VERSION 1U
#define SW_MAX_PAYLOAD 48U
#define SW_MAX_FRAME (SW_MAX_PAYLOAD + 9U)
#define SW_SNAPSHOT_BYTES 40U
#define SW_REQUEST_BYTES 16U
#define SW_PERIOD_MS 200U
#define SW_TIMEOUT_MS 2000U
enum { SW_HEARTBEAT = 1, SW_SNAPSHOT = 2, SW_REQUEST = 3, SW_RESULT = 4, SW_CAL_REQUEST = 5, SW_CAL_RESULT = 6 };
enum { SW_OK = 0, SW_REJECTED = 1, SW_STALE = 2, SW_BAD_VALUE = 3,
       SW_BUSY = 4, SW_NO_LINK = 5 };
typedef void (*SwReceive)(uint8_t type, uint16_t sequence, const uint8_t *payload, uint8_t size);
typedef struct {
    uint8_t bytes[SW_MAX_FRAME];
    size_t used;
    uint32_t bad_crc;
    uint32_t bad_length;
} SwParser;
uint16_t Sw_Get16(const uint8_t *p);
uint32_t Sw_Get32(const uint8_t *p);
void Sw_Put16(uint8_t *p, uint16_t v);
void Sw_Put32(uint8_t *p, uint32_t v);
uint16_t Sw_Crc(const uint8_t *data, size_t length);
size_t Sw_Encode(uint8_t *out, uint8_t type, uint16_t sequence, const uint8_t *payload, uint8_t size);
void Sw_Feed(SwParser *parser, const uint8_t *data, size_t length, SwReceive receive);
bool Sw_IsNewer(uint16_t candidate, uint16_t previous);
/* Retained across warm reset; identity is not an authentication credential. */
uint32_t Sw_BootId(uint32_t seed);
#endif
