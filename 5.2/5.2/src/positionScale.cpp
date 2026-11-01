#include "positionScale.h"

const char *positionReadStatusName(PositionReadStatus status)
{
    switch (status)
    {
        case POSITION_READ_OK:       return "OK";
        case POSITION_READ_AT_LIMIT: return "LIMITA";
    }
    return "?";
}

static int16_t clampRaw(int16_t raw)
{
    if (raw < ADC_RAW_MIN) return ADC_RAW_MIN;
    if (raw > ADC_RAW_MAX) return ADC_RAW_MAX;
    return raw;
}

int16_t rawToPositionDeciDeg(int16_t raw)
{
    int32_t scaled = (int32_t)clampRaw(raw) * POSITION_MAX_DECI_DEG;
    return (int16_t)(scaled / ADC_RAW_MAX);
}

bool isRawAtLimit(int16_t raw)
{
    return raw <= ADC_RAW_MIN + ADC_LIMIT_MARGIN ||
           raw >= ADC_RAW_MAX - ADC_LIMIT_MARGIN;
}

PositionSample buildPositionSample(int16_t raw)
{
    PositionSample sample;

    sample.positionDeciDeg = rawToPositionDeciDeg(raw);
    sample.status = isRawAtLimit(raw) ? POSITION_READ_AT_LIMIT : POSITION_READ_OK;
    return sample;
}