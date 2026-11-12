#ifndef HOME_REFERENCE_H
#define HOME_REFERENCE_H
#include <stdbool.h>
#include <stdint.h>
/* Right switch status, right position latch, or right stop event. */
static inline bool HomeReference_Triggered(uint32_t status)
{
    return (status & 0x2AU) != 0U;
}
/* Zero alone is insufficient. Once triggered, switch release is permitted. */
static inline bool HomeReference_Stopped(bool triggered, uint32_t status)
{
    return triggered && (status & 0x400U) != 0U;
}
#endif
