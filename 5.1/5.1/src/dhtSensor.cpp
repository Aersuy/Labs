#include "dhtSensor.h"
#include "deciTemperature.h"
#include <math.h>

static const uint8_t DHT_MODEL = DHT11;
static const float DECI_SCALE = (float)DECI_PER_UNIT;
static const float ROUNDING_OFFSET = 0.5;

const char *dhtReadStatusName(DhtReadStatus status)
{
    switch (status)
    {
        case DHT_READ_OK:            return "OK";
        case DHT_READ_FAILED:        return "FAILED";
        case DHT_READ_OUT_OF_RANGE:  return "OUT_OF_RANGE";
        default:                     return "UNKNOWN";
    }
}

DhtSensor::DhtSensor(int pin)
    : driver((uint8_t)pin, DHT_MODEL)
{
}

void DhtSensor::begin()
{
    this->driver.begin();
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

static DhtReadStatus classifyRange(int16_t temperatureDeciC,
                                   int16_t humidityDeciPercent)
{
    bool isTemperatureValid = isWithin(temperatureDeciC,
                                       DhtSensor::TEMPERATURE_MIN_DECI_C,
                                       DhtSensor::TEMPERATURE_MAX_DECI_C);
    bool isHumidityValid = isWithin(humidityDeciPercent,
                                    DhtSensor::HUMIDITY_MIN_DECI_PERCENT,
                                    DhtSensor::HUMIDITY_MAX_DECI_PERCENT);

    return (isTemperatureValid && isHumidityValid) ? DHT_READ_OK
                                                   : DHT_READ_OUT_OF_RANGE;
}

DhtSample DhtSensor::read()
{
    DhtSample sample;
    float temperature = this->driver.readTemperature();
    float humidity = this->driver.readHumidity();

    if (isnan(temperature) || isnan(humidity))
    {
        sample.temperatureDeciC = 0;
        sample.humidityDeciPercent = 0;
        sample.status = DHT_READ_FAILED;
        return sample;
    }

    sample.temperatureDeciC = toDeci(temperature);
    sample.humidityDeciPercent = toDeci(humidity);
    sample.status = classifyRange(sample.temperatureDeciC,
                                  sample.humidityDeciPercent);
    return sample;
}