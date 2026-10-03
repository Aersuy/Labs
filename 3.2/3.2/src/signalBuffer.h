#ifndef SIGNALBUFFER_H
#define SIGNALBUFFER_H
#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>


class SignalBuffer
{
private:
    int *samples;
    size_t index;
    size_t size;
    SemaphoreHandle_t mutex;
public:
    SignalBuffer(size_t size = 50);
    ~SignalBuffer();
    void init();
    void pushSample(int raw);              // called by vTask1, every 10ms
    void copyOut(int *dest, size_t n);      // called by vTask3, every 500ms
};
#endif