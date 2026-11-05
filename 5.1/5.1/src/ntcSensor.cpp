#include "ntcSensor.h"
#include "deciTemperature.h"
#include <math.h>

// Matches the Wokwi NTC part (10K thermistor in series with a 10K resistor,
// beta 3950) and the physical KY-013 on the bench. These constants are
// mirrored in chips/peltier.chip.c, which generates the voltage this class
// reads back; if the two drift apart the loop picks up a constant offset.
static const float R_FIXED = 10000.0;   // divider resistor
static const float R0 = 10000.0;        // thermistor resistance at T0
static const float T0_KELVIN = 298.15;  // 25 C
static const float BETA = 3950.0;
static const float ADC_MAX = 1023.0;

static const float KELVIN_OFFSET = 273.15;
static const float DECI_SCALE = (float)DECI_PER_UNIT;
static const float ROUNDING_OFFSET = 0.5;

const char *ntcReadStatusName(NtcReadStatus status)
{
    switch (status)
    {
        case NTC_READ_OK:            return "OK";
        case NTC_READ_FAILED:        return "FAILED";
        case NTC_READ_OUT_OF_RANGE:  return "OUT_OF_RANGE";
        default:                     return "UNKNOWN";
    }
}

NtcSensor::NtcSensor(int pin)
{
    this->memberPin = pin;
}

void NtcSensor::begin()
{
    pinMode(this->memberPin, INPUT);
}

// Rounds half away from zero, so 24.75 C becomes 248 and not 247.
static int16_t toDeci(float value)
{
    float scaled = value * DECI_SCALE;
    return (int16_t)((scaled >= 0) ? scaled + ROUNDING_OFFSET
                                   : scaled - ROUNDING_OFFSET);
}

static bool isWithin(int16_t value, int16_t minimum, int16_t maximum)
{
    return value >= minimum && value <= maximum;
}

// Beta equation on the divider, as in lab 3.1: the thermistor sits on the low
// side, so the raw count climbs with its resistance and falls as it warms.
static float celsiusFromRaw(int raw)
{
    float resistance = R_FIXED * ((float)raw / (ADC_MAX - (float)raw));
    float kelvin = 1.0 / (1.0 / T0_KELVIN + log(resistance / R0) / BETA);
    return kelvin - KELVIN_OFFSET;
}

NtcSample NtcSensor::read()
{
    NtcSample sample;
    int raw = analogRead(this->memberPin);

    // A railed converter means the divider is broken, not that the room is
    // impossibly cold: the beta equation would divide by zero or take the log
    // of zero here, so refuse the sample and let the fail-safe path run.
    if (raw <= 0 || raw >= (int)ADC_MAX)
    {
        sample.temperatureDeciC = 0;
        sample.status = NTC_READ_FAILED;
        return sample;
    }

    sample.temperatureDeciC = toDeci(celsiusFromRaw(raw));
    sample.status = isWithin(sample.temperatureDeciC,
                             NtcSensor::TEMPERATURE_MIN_DECI_C,
                             NtcSensor::TEMPERATURE_MAX_DECI_C)
                    ? NTC_READ_OK
                    : NTC_READ_OUT_OF_RANGE;
    return sample;
}
