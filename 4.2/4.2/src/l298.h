#ifndef L298_H
#define L298_H

#include <Arduino.h>

// Direction as seen by one L298 half-bridge channel
typedef enum {
    L298_DIR_STOP = 0,
    L298_DIR_FORWARD,
    L298_DIR_BACKWARD
} L298Direction;


class L298Channel {
    public:
    L298Channel(int in1Pin, int in2Pin, int enablePin);

    void begin();
    void setDirection(L298Direction direction);
    void setDuty(uint8_t duty);            
    void coast();                          
    void brake();                          

    L298Direction getDirection() const;
    uint8_t getDuty() const;

    static const uint8_t PWM_DUTY_MAX = 255;

    private:
    void writeDirectionPins(bool in1Level, bool in2Level);

    int in1Pin;
    int in2Pin;
    int enablePin;
    L298Direction direction;
    uint8_t duty;
};

#endif