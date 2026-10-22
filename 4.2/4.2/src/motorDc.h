#ifndef MOTORDC_H
#define MOTORDC_H

#include <Arduino.h>
#include "l293d.h"

// driver de nivel mijlociu, lucrea
class MotorDc {
    public:
    MotorDc(int in1Pin, int in2Pin, int enablePin);

    void begin();

    bool setPower(int percent);     // -100 - 100
    void stop();                    // power 0
    void setMaxPower();             // 100 % 
    bool increasePower();           // + 10 %
    bool decreasePower();           // - 10 %

    int getPower() const;                   // %
    const char *directionName() const;      // "FORWARD" / "BACKWARD" / "STOP"

    static const int POWER_MAX = 100;
    static const int POWER_MIN = -100;
    static const int POWER_STEP = 10;

    private:
    void applyPower();
    static uint8_t percentToDuty(int absolutePercent);

    L293DChannel channel;
    int power;              // percentage -100 % ---- 100 %
    int lastDirectionSign;  // +1 / -1,
};
#endif