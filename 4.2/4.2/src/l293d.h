#pragma once
#include <Arduino.h>

// Direction as seen by one L293D half-bridge pair
typedef enum {
    L293D_DIR_STOP = 0,
    L293D_DIR_FORWARD,
    L293D_DIR_BACKWARD
} L293DDirection;


class L293DChannel {
    public:
    L293DChannel(int in1Pin, int in2Pin, int enablePin);

    void begin();
    void setDirection(L293DDirection direction);
    void setDuty(uint8_t duty);
    void coast();                          // enable low, shaft free
    void brake();                          // terminals shorted

    L293DDirection getDirection() const;
    uint8_t getDuty() const;

    static const uint8_t PWM_DUTY_MAX = 255;

    private:
    void writeDirectionPins(bool in1Level, bool in2Level);

    int in1Pin;
    int in2Pin;
    int enablePin;
    L293DDirection direction;
    uint8_t duty;
};
