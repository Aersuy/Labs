#include "signalBuffer.h"

SignalBuffer::SignalBuffer(size_t size)
{
    this->size = size;
    this->samples = new int[size];
    this->index = 0;
}
SignalBuffer::~SignalBuffer()
{
    delete[] this->samples;
}
void SignalBuffer::init()
{
    this->mutex = xSemaphoreCreateMutex();
}
void SignalBuffer::pushSample(int sample)
{  
     xSemaphoreTake(this->mutex,portMAX_DELAY);
    samples[this->index] = sample;
    if (this->index >= this->size - 1)
    {
        this->index = 0;
    } else
    {
        this->index++;
    }
    xSemaphoreGive(this->mutex);   
}
void SignalBuffer::copyOut(int *dest, size_t n)
{
    xSemaphoreTake(this->mutex, portMAX_DELAY);
    for (size_t i = 0; i < n; i++)
    {
        dest[i] = this->samples[i];
    }
    xSemaphoreGive(this->mutex);
}