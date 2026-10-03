#include "signal.h"
const char *signalStatusStr(SensorStatus s)
{
    switch (s)
    {
        case SENSOR_OK:           return "OK";
        case SENSOR_ERROR_RANGE:  return "ERROR_RANGE";
        default:                  return "UNKNOWN";
    }
}
Signal::Signal(float temp,SensorStatus status,uint16_t count)
{
    this->value = temp;
    this->status = status;
    this->sampleCount = count;
}
void Signal::signalInit()
{
    this->mutex = xSemaphoreCreateMutex();
}

void Signal::signalUpdate(float temp,SensorStatus status)
{
    xSemaphoreTake(mutex,portMAX_DELAY);
    this->value = temp;
    this->status = status;
    this->sampleCount++;
    xSemaphoreGive(mutex);
}
void Signal::getSignals(Signal *out)
{
    xSemaphoreTake(mutex,portMAX_DELAY);
    out->value = this->value;
    out->status = this->status;
    out->sampleCount = this->sampleCount;
    xSemaphoreGive(mutex);
}
float Signal::getValue()
{
    return this->value;
}
SensorStatus Signal::getStatus()
{
    return this->status;
}