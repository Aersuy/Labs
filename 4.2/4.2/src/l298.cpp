#include "l298.h"

// pins and logical state
L298Channel::L298Channel(int in1Pin, int in2Pin, int enablePin)
{
    this->in1Pin = in1Pin;
    this->in2Pin = in2Pin;
    this->enablePin = enablePin;
    this->direction = L298_DIR_STOP;
    this->duty = 0;
}

// configures pins
void L298Channel::begin()
{
    pinMode(this->in1Pin, OUTPUT);
    pinMode(this->in2Pin, OUTPUT);
    pinMode(this->enablePin, OUTPUT);
    this->coast();
}
void L298Channel::writeDirectionPins(bool in1Level, bool in2Level)
{
    digitalWrite(this->in1Pin, in1Level ? HIGH : LOW);
    digitalWrite(this->in2Pin, in2Level ? HIGH : LOW);
}

void L298Channel::setDirection(L298Direction direction)
{
    this->direction = direction;
    switch (direction)
    {
        case L298_DIR_FORWARD:  this->writeDirectionPins(true, false); break;
        case L298_DIR_BACKWARD: this->writeDirectionPins(false, true); break;
        case L298_DIR_STOP:
        default:                this->writeDirectionPins(false, false); break;
    }
}

void L298Channel::setDuty(uint8_t duty)
{
    this->duty = duty;
    analogWrite(this->enablePin, duty);
}

void L298Channel::coast()
{
    this->setDuty(0);
    this->setDirection(L298_DIR_STOP);
}

void L298Channel::brake()
{
    this->writeDirectionPins(true, true);
    this->direction = L298_DIR_STOP;
    this->setDuty(L298Channel::PWM_DUTY_MAX);
}

L298Direction L298Channel::getDirection() const
{
    return this->direction;
}

uint8_t L298Channel::getDuty() const
{
    return this->duty;
}