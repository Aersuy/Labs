#ifndef NTCSENSOR_H
#define NTCSENSOR_H

#include <Arduino.h>
#include <stdint.h>

typedef enum {
    NTC_READ_OK = 0,
    NTC_READ_FAILED,          // ADC railed: thermistor open or shorted
    NTC_READ_OUT_OF_RANGE     // plausible reading, but outside the work range
} NtcReadStatus;

const char *ntcReadStatusName(NtcReadStatus status);

typedef struct {
    int16_t temperatureDeciC;
    NtcReadStatus status;
} NtcSample;

// Wraps the NTC divider so no other module has to know about beta equations,
// and hands out tenths of a degree instead of floats.
class NtcSensor {
    public:
    explicit NtcSensor(int pin);

    void begin();
    NtcSample read();

    // An NTC has no datasheet refresh limit, unlike the DHT11 this replaced,
    // so the loop can run fast enough to make the hysteresis band visible.
    static const uint16_t SAMPLE_INTERVAL_MS = 200;
    static const int16_t TEMPERATURE_MIN_DECI_C = 0;
    static const int16_t TEMPERATURE_MAX_DECI_C = 500;

    private:
    int memberPin;
};

#endif
