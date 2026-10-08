#include "ky018.h"

// Arduino max digital voltage
static const float ADC_MAX = 1023.0;

//Constructor for the class
ky018::ky018(int pin)
{
    this->pin = pin;
}
void ky018::ky018Init()
{
}

// reads the the raw value from the analog pig
int ky018::readRaw()
{
    return analogRead(this->pin);
}

// Returns the light percentage based on the raw 
// analog value.
float ky018::readLightPercent()
{
    return (readRaw() / ADC_MAX) * 100.0;
}