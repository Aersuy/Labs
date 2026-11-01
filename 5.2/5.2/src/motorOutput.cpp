#include "motorOutput.h"

static int absoluteValue(int value)
{
    return (value < 0) ? -value : value;
}

static int clampMagnitude(int magnitude)
{
    return (magnitude > PER_MILLE_MAX) ? PER_MILLE_MAX : magnitude;
}

// A hard floor (max(|p|, MIN)) would make the shaft jump as the output leaves
// zero; remapping the whole range keeps the actuator continuous instead.
static int magnitudeToPercent(int magnitude)
{
    const int span = MOTOR_MAX_PERCENT - MOTOR_MIN_MOVE_PERCENT;
    long scaled = ((long)magnitude * span) / PER_MILLE_MAX;
    return MOTOR_MIN_MOVE_PERCENT + (int)scaled;
}

int perMilleToMotorPercent(int16_t outputPerMille)
{
    if (outputPerMille == 0) return 0;

    int magnitude = clampMagnitude(absoluteValue((int)outputPerMille));
    int percent = magnitudeToPercent(magnitude);
    return (outputPerMille > 0) ? percent : -percent;
}