#ifndef TMP112_SAMPLE_H
#define TMP112_SAMPLE_H
#include <stdbool.h>
#include <stdint.h>
/* TI TMP112: R1:R0=11, SD=0, EM=0, reserved low bits=0.
 * Require CR1=1 (4/8 Hz); a stopped or slow converter is not a fresh control input.
 * AL/OS are dynamic and deliberately excluded from the mask. */
static inline bool Tmp112_ConfigValid(uint16_t config)
{
    return (config & 0x611fU) == 0x6000U && (config & 0x0080U) != 0U;
}
static inline bool Tmp112_Decode(uint16_t word, float *temperature)
{
    if (!temperature || (word & 0x000fU)) return false;
    int16_t code = (int16_t)word;
    float value = (float)(code / 16) * 0.0625f;
    if (value <= 0.0f || value > 125.0f) return false;
    *temperature = value;
    return true;
}
#endif
