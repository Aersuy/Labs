#include "speedScale.h"

// pulses / PULSES_PER_REV revolutions over elapsed minutes, in tenths:
//
//   deciRpm = pulses * 10 * 60000 / (PULSES_PER_REV * elapsedMs)
//
// The interval is reduced to milliseconds first, with rounding. Keeping it in
// microseconds would need a 64-bit numerator for anything past three pulses,
// and the AVR pays for that division every sample; a rounded millisecond over
// a ~96 ms window costs well under 1 %, which is far below the noise the
// gearbox itself contributes.
static const uint32_t DECI_RPM_NUMERATOR = 600000UL;

const char *speedReadStatusName(SpeedReadStatus status)
{
    switch (status)
    {
        case SPEED_READ_OK:         return "OK";
        case SPEED_READ_STALLED:    return "BLOCAT";
        case SPEED_READ_OVER_RANGE: return "PESTE_SCALA";
    }
    return "?";
}

static uint32_t microsToMillisRounded(uint32_t micros)
{
    uint32_t millis = (micros + 500UL) / 1000UL;
    return (millis == 0) ? 1UL : millis;
}

int16_t pulsesToDeciRpm(uint16_t pulses, uint32_t elapsedMicros)
{
    if (pulses == 0) return 0;

    uint32_t elapsedMillis = microsToMillisRounded(elapsedMicros);
    uint32_t deciRpm = ((uint32_t)pulses * DECI_RPM_NUMERATOR) /
                       ((uint32_t)ENCODER_PULSES_PER_REV * elapsedMillis);

    if (deciRpm > (uint32_t)SPEED_MAX_DECI_RPM) return SPEED_MAX_DECI_RPM;
    return (int16_t)deciRpm;
}

SpeedSample buildSpeedSample(const EncoderDelta *delta)
{
    SpeedSample sample;

    // No new edge: either the shaft is genuinely stopped, or it is turning so
    // slowly that this interval saw nothing. Both read as zero; only the
    // timeout separates the stall from the crawl.
    if (delta->pulses == 0 || delta->elapsedMicros == 0)
    {
        sample.speedDeciRpm = 0;
        sample.status = (delta->sinceLastEdgeMicros >= SPEED_STALL_TIMEOUT_MICROS)
                            ? SPEED_READ_STALLED
                            : SPEED_READ_OK;
        return sample;
    }

    uint32_t elapsedMillis = microsToMillisRounded(delta->elapsedMicros);
    uint32_t deciRpm = ((uint32_t)delta->pulses * DECI_RPM_NUMERATOR) /
                       ((uint32_t)ENCODER_PULSES_PER_REV * elapsedMillis);

    if (deciRpm > (uint32_t)SPEED_MAX_DECI_RPM)
    {
        sample.speedDeciRpm = SPEED_MAX_DECI_RPM;
        sample.status = SPEED_READ_OVER_RANGE;
        return sample;
    }

    sample.speedDeciRpm = (int16_t)deciRpm;
    sample.status = SPEED_READ_OK;
    return sample;
}
