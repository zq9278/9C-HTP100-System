#ifndef PRESSURE_MOTION_H
#define PRESSURE_MOTION_H
#include <math.h>
#include <stdbool.h>
/* Continuous pure-P command bounded by the approach envelope. No stage jump. */
static inline float PressureMotion_Command(float target, float pressure, float fast,
                                          float reverse, float kp, float ratio, bool hold)
{
    float error = target - pressure;
    if (hold && fabsf(error) <= 3.0f) return 0.0f;
    float envelope = fast * fabsf(error) / fmaxf(target * ratio, 1.0f);
    if (envelope > fast) envelope = fast;
    float command = kp * error;
    if (command > envelope) command = envelope;
    if (command < -reverse) command = -reverse;
    return command;
}
/* Emergency stops and reductions are immediate; only magnitude increases ramp. */
static inline float PressureMotion_Slew(float previous, float desired, float dt, float rate)
{
    if (previous * desired < 0.0f) return 0.0f;
    float magnitude = fabsf(previous) + fmaxf(dt, 0.0f) * rate;
    return fminf(fmaxf(desired, -magnitude), magnitude);
}
#endif
