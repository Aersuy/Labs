#include "potPositionSensor.h"

PotPositionSensor::PotPositionSensor(int pin)
{
    this->pin = pin;
}

void PotPositionSensor::begin()
{
    pinMode(this->pin, INPUT);
}

PositionSample PotPositionSensor::read()
{
    return buildPositionSample((int16_t)analogRead(this->pin));
}