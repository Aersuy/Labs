#ifndef SIGNALS_H
#define SIGNALS_H
#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>

enum SensorStatus {
    SENSOR_OK,
    SENSOR_ERROR_RANGE
};
const char *signalStatusStr(SensorStatus s);
class Signal
{
private:
    float value;
    SensorStatus status;
    uint16_t sampleCount;
    SemaphoreHandle_t mutex;
public:
    Signal(float initTemp = 0.0,SensorStatus status = SENSOR_OK,uint16_t count = 0);
    void signalInit();
    void signalUpdate(float temp, SensorStatus statusV);
    void getSignals(Signal *out);
    float getValue();
    SensorStatus getStatus();

};



#endif