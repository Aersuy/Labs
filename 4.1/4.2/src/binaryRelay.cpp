#include "binaryRelay.h"

// constructor that takes the pin, logical level, protection timing
// and initialises the logical states
Relay::Relay(int pin, uint8_t activeLevel, unsigned long minSwitchInterval)
{
    this->memberPin = pin;
    this->activeLevel = activeLevel;
    this->minSwitchInterval = minSwitchInterval;
    this->lastSwitchTime = 0;
    this->state = false;
}
// begin, inits to false before pinmode to avoid 
// relay getting unneded input
// inits more logical states
void Relay::begin()
{
   this->write(false);
   pinMode(this->memberPin, OUTPUT);
   this->write(false);
   this->state = false;
   this->lastSwitchTime = millis();
}

// actual interaction with the board
// as some boards may have reversed polarity, uses active level
void Relay::write(bool active)
{
   uint8_t level = active ? this->activeLevel
   : (this->activeLevel == HIGH ? LOW : HIGH);
   digitalWrite(this->memberPin, level);
}

bool Relay::isOn() const
{
   return this->state;
}

// easy printing
const char *Relay::stateName() const
{
   return this->state ? "ON" : "OFF";
}

// safety mechanism
bool Relay::canSwitch() const
{
   return (millis() - this->lastSwitchTime) >= this->minSwitchInterval;
}

// turns on if it can
bool Relay::turnOn()
{
   if (this->isOn()) return true;
   if (!this->canSwitch()) return false;

   this->write(true);
   this->state = true;
   this->lastSwitchTime = millis();
   return true;
}

// turns off if it can
bool Relay::turnOff()
{
   if (!this->isOn()) return true;
   if (!this->canSwitch()) return false;

   this->write(false);
   this->state = false;
   this->lastSwitchTime = millis();
   return true;
}

// reverse state
bool Relay::toggle()
{
   return this->isOn() ? this->turnOff() : this->turnOn();
}