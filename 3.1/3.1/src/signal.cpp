#include "signal.h"


// Function that returns the status of the signal
const char *signalStatusStr(SensorStatus s)
{
    switch (s)
    {
        case SENSOR_OK:           return "OK";
        case SENSOR_ERROR_RANGE:  return "ERROR_RANGE";
        default:                  return "UNKNOWN";
    }
}

// signal construction, initialises with either input values
// or 0-roed
Signal::Signal(float temp,SensorStatus status,uint16_t count)
{
    this->value = temp;
    this->status = status;
    this->sampleCount = count;
}
// init method, creates the mutex
void Signal::signalInit()
{
    this->mutex = xSemaphoreCreateMutex();
}

// updates the values of the signal, uses the mutex to prevent weirdness
void Signal::signalUpdate(float temp,SensorStatus status)
{
    xSemaphoreTake(mutex,portMAX_DELAY);
    this->value = temp;
    this->status = status;
    this->sampleCount++;
    xSemaphoreGive(mutex);
}
// returns a pointer to a new signal object
// used to give the values of the current signal
// without touching the mutex
void Signal::getSignals(Signal *out)
{
    xSemaphoreTake(mutex,portMAX_DELAY);
    out->value = this->value;
    out->status = this->status;
    out->sampleCount = this->sampleCount;
    xSemaphoreGive(mutex);
}

// Getters for value and status
float Signal::getValue()
{
    return this->value;
}
SensorStatus Signal::getStatus()
{
    return this->status;
}