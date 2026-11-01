#include "l293d.h"

// pins and logical state
L293DChannel::L293DChannel(int in1Pin, int in2Pin, int enablePin)
{
    this->in1Pin = in1Pin;
    this->in2Pin = in2Pin;
    this->enablePin = enablePin;
    this->direction = L293D_DIR_STOP;
    this->duty = 0;
}

// configures pins
void L293DChannel::begin()
{
    pinMode(this->in1Pin, OUTPUT);
    pinMode(this->in2Pin, OUTPUT);
    pinMode(this->enablePin, OUTPUT);
    this->coast();
}

void L293DChannel::writeDirectionPins(bool in1Level, bool in2Level)
{
    digitalWrite(this->in1Pin, in1Level ? HIGH : LOW);
    digitalWrite(this->in2Pin, in2Level ? HIGH : LOW);
}

void L293DChannel::setDirection(L293DDirection direction)
{
    this->direction = direction;
    switch (direction)
    {
        case L293D_DIR_FORWARD:  this->writeDirectionPins(true, false); break;
        case L293D_DIR_BACKWARD: this->writeDirectionPins(false, true); break;
        case L293D_DIR_STOP:
        default:                 this->writeDirectionPins(false, false); break;
    }
}

void L293DChannel::setDuty(uint8_t duty)
{
    this->duty = duty;
    analogWrite(this->enablePin, duty);
}

void L293DChannel::coast()
{
    this->setDuty(0);
    this->setDirection(L293D_DIR_STOP);
}

void L293DChannel::brake()
{
    this->writeDirectionPins(true, true);
    this->direction = L293D_DIR_STOP;
    this->setDuty(L293DChannel::PWM_DUTY_MAX);
}

L293DDirection L293DChannel::getDirection() const
{
    return this->direction;
}

uint8_t L293DChannel::getDuty() const
{
    return this->duty;
}
