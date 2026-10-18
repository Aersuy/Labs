#include "ky023.h"

// Arduino max analog reading
static const float ADC_MAX = 1023.0;

// Constructor for the class
ky023::ky023(int pinX, int pinY, int pinSW)
{
    this->pinX = pinX;
    this->pinY = pinY;
    this->pinSW = pinSW;
}
void ky023::ky023Init()
{
    // button pulls low when pressed, idles high
    pinMode(this->pinSW, INPUT_PULLUP);
}

// reads the raw X axis value from the analog pin
int ky023::readRawX()
{
    return analogRead(this->pinX);
}

// reads the raw Y axis value from the analog pin
int ky023::readRawY()
{
    return analogRead(this->pinY);
}

// Converts a raw (or conditioned) analog axis value into a
// 0-100% position, same scaling style as ky018's light percent.
float ky023::rawToPercent(float raw)
{
    if (raw >= ADC_MAX) raw = ADC_MAX;
    if (raw <= 0.0)     raw = 0.0;

    return (raw / ADC_MAX) * 100.0;
}

// reads the button state directly, no buffering/filtering -
// it's an instantaneous press, not a noisy analog signal
bool ky023::isPressed()
{
    return digitalRead(this->pinSW) == LOW;
}
