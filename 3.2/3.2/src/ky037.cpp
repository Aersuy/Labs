#include "ky037.h"
#include <math.h>   // log10()

// Arduino max analog reading
static const float ADC_MAX = 1023.0;

// Constructor for the class
ky037::ky037(int pin)
{
    this->pin = pin;
}
void ky037::ky037Init()
{
}

// reads the raw value from the analog pin
int ky037::readRaw()
{
    return analogRead(this->pin);
}

// Converts a raw (or conditioned) analog value into a
// relative sound level in dB. The KY-037 has no calibrated
// SPL reference, so this is a logarithmic scaling of the
// raw envelope amplitude (0-1023), not an absolute SPL reading.
float ky037::rawToDb(float raw)
{
    if (raw < 1.0) raw = 1.0;   // avoid log10(0)

    return 20.0 * log10(raw / ADC_MAX) + 90.0;   // shift into a readable positive range
}