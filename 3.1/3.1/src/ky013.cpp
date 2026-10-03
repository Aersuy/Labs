#include "ky013.h"
#include <math.h>   // log()

static const float R_FIXED = 10000.0;  // divder resister
static const float R0 = 10000.0;       // thermalresistor resistence at 25c
static const float T0 = 298.15;        // 25°C kevin
static const float B  = 3950.0;        // KY-013 beta coefficient
static const float ADC_MAX = 1023.0;
ky013::ky013(int pin)
{
    this->pin = pin;
}
void ky013::ky013Init()
{
}
float ky013::readKelvin()
{
    int raw = analogRead(this->pin);

    if (raw >= ADC_MAX) raw = ADC_MAX - 1.0;
    if (raw <= 0.0)     raw = 1.0;
    float  r = R_FIXED * (ADC_MAX / raw - 1.0); // Get the resistence of the thermoresistor
    return 1.0 / (1.0/T0 + (1.0/B) * log(r / R0)); // use the beta equation to calculate temp
}
float ky013::readCelcius()
{
    return readKelvin() - 273.15;
}