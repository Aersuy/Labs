#include "ky018.h"

static const float ADC_MAX = 1023.0;

ky018::ky018(int pin)
{
    this->pin = pin;
}
void ky018::ky018Init()
{
}
int ky018::readRaw()
{
    return analogRead(this->pin);
}
float ky018::readLightPercent()
{
    return (readRaw() / ADC_MAX) * 100.0;
}