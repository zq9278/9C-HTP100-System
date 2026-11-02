#include "system_wire.h"
#include <string.h>
uint16_t Sw_Get16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8); }
uint32_t Sw_Get32(const uint8_t *p) { return (uint32_t)Sw_Get16(p) | ((uint32_t)Sw_Get16(p + 2) << 16); }
void Sw_Put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
void Sw_Put32(uint8_t *p, uint32_t v) { Sw_Put16(p, (uint16_t)v); Sw_Put16(p + 2, (uint16_t)(v >> 16)); }
uint16_t Sw_Crc(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffff;
    while (length--) {
        crc ^= *data++;
        for (unsigned b = 0; b < 8; b++) crc = (uint16_t)((crc >> 1) ^ ((crc & 1) ? 0xa001 : 0));
    }
    return crc;
}
bool Sw_IsNewer(uint16_t candidate, uint16_t previous) { return (int16_t)(candidate - previous) > 0; }
size_t Sw_Encode(uint8_t *out, uint8_t type, uint16_t sequence, const uint8_t *payload, uint8_t size)
{
    if (!out || size > SW_MAX_PAYLOAD || (size && !payload)) return 0;
    out[0] = 0xb5; out[1] = 0x5b; out[2] = SW_VERSION; out[3] = type;
    Sw_Put16(out + 4, sequence); out[6] = size;
    if (size) memcpy(out + 7, payload, size);
    Sw_Put16(out + 7 + size, Sw_Crc(out, 7 + size));
    return 9U + size;
}
void Sw_Feed(SwParser *p, const uint8_t *data, size_t length, SwReceive receive)
{
    if (!p || !data || !receive) return;
    while (length--) {
        p->bytes[p->used++] = *data++;
        while (p->used) {
            if (p->bytes[0] != 0xb5 || (p->used > 1 && p->bytes[1] != 0x5b)) goto discard;
            if (p->used < 7) break;
            if (p->bytes[2] != SW_VERSION || p->bytes[6] > SW_MAX_PAYLOAD) { p->bad_length++; goto discard; }
            size_t total = 9U + p->bytes[6];
            if (p->used < total) break;
            if (Sw_Get16(p->bytes + total - 2) != Sw_Crc(p->bytes, total - 2)) { p->bad_crc++; goto discard; }
            receive(p->bytes[3], Sw_Get16(p->bytes + 4), p->bytes + 7, p->bytes[6]);
            memmove(p->bytes, p->bytes + total, p->used - total); p->used -= total;
            continue;
discard:
            memmove(p->bytes, p->bytes + 1, --p->used);
        }
    }
}
uint32_t Sw_BootId(uint32_t seed)
{
    static struct { uint32_t id, inverse; } retained __attribute__((section(".noinit")));
    uint32_t id = retained.id;
    if ((id ^ retained.inverse) != UINT32_MAX) id = seed;
    id++; if (!id) id = 1;
    retained.id = id; retained.inverse = ~id;
    return id;
}
