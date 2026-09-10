#include "ledClass.h"
//Constructor for Led class, initialises memberPin
//with the chosen pin, sets the pinMode, set the initial
// output to low and sets the logical state to off
Led::Led(int pin)
{
  this->memberPin = pin;
  pinMode(this->memberPin,OUTPUT);
  digitalWrite(this->memberPin,LOW);
  this->state = false;
}
//Checks if the led is on
bool Led::isOn()
{
  return this->state;
}
//Turns on the led, does nothing if already on
void Led::turnOn()
{
  if(!this->isOn())
  {
    digitalWrite(this->memberPin,HIGH);
    this->state = true;
  }
}
//Turns offf the led, does nothing if already off
void Led::turnOff()
{
  if(this->isOn())
  {
    digitalWrite(this->memberPin,LOW);
    this->state = false;
  }
}
//Inverses the led state
void Led::toggle()
{
  if(this->isOn())
  {
    this->turnOff();
  } else
  {
    this->turnOn();
  }
}