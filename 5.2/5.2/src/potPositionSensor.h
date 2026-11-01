#ifndef POTPOSITIONSENSOR_H
#define POTPOSITIONSENSOR_H

#include <Arduino.h>
#include "positionScale.h"

// The only module that touches the ADC. All conversion lives in positionScale,
// so the maths stays deterministic and testable on the host.
class PotPositionSensor {
    public:
    explicit PotPositionSensor(int pin);

    void begin();
    PositionSample read();

    private:
    int pin;
};

#endif