#ifndef DHTSENSOR_H
#define DHTSENSOR_H

#include <Arduino.h>
#include <DHT.h>
#include <stdint.h>

typedef enum {
    DHT_READ_OK = 0,
    DHT_READ_FAILED,          // no answer / bad checksum: sensor unplugged
    DHT_READ_OUT_OF_RANGE     // answered, but outside what a DHT11 can mean
} DhtReadStatus;

const char *dhtReadStatusName(DhtReadStatus status);

typedef struct {
    int16_t temperatureDeciC;
    int16_t humidityDeciPercent;
    DhtReadStatus status;
} DhtSample;

// Wraps the DHT library so no other module has to know it exists, and hands
// out tenths instead of floats.
class DhtSensor {
    public:
    explicit DhtSensor(int pin);

    void begin();
    DhtSample read();

    // DHT11 datasheet: one reading per second at most; 2 s keeps a margin.
    static const uint16_t MIN_READ_INTERVAL_MS = 2000;
    static const int16_t TEMPERATURE_MIN_DECI_C = 0;
    static const int16_t TEMPERATURE_MAX_DECI_C = 500;
    static const int16_t HUMIDITY_MIN_DECI_PERCENT = 200;
    static const int16_t HUMIDITY_MAX_DECI_PERCENT = 900;

    private:
    DHT driver;
};

#endif