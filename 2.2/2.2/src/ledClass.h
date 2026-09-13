#ifndef LEDCLASS_H
#define LEDCLASS_H

#include <Arduino.h>

class Led {
  public:
    Led(int pin);
    void begin();
    void turnOn();
    void turnOff();
    void toggle();
    bool isOn();
  private:
    int memberPin;
    bool state;
};
#endif