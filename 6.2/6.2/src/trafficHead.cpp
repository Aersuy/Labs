#include "trafficHead.h"

TrafficHead::TrafficHead(int redPin, int yellowPin, int greenPin)
  : red(redPin), yellow(yellowPin), green(greenPin)
{
}

void TrafficHead::begin()
{
  this->red.begin();
  this->yellow.begin();
  this->green.begin();
}

// Stinge toate culorile inainte de a aprinde una, ca doua lampi sa nu poata
// fi aprinse simultan nici pentru o clipa.
void TrafficHead::show(LampColor color)
{
  this->red.turnOff();
  this->yellow.turnOff();
  this->green.turnOff();

  switch (color)
  {
    case LAMP_RED:    this->red.turnOn();    break;
    case LAMP_YELLOW: this->yellow.turnOn(); break;
    case LAMP_GREEN:  this->green.turnOn();  break;
    case LAMP_OFF:    break;
  }
}
