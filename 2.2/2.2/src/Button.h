#ifndef BUTTON_CLASS_H
#define BUTTON_CLASS_H
#include <Arduino.h>
class Button
{
public:
  Button(int pin, unsigned long debounceDelay = 20);
  void begin();
  void update();
  bool wasPressed();
private:
  bool state;
  bool lastState;
  int pin;
  bool pressed;

  bool rawState;
  unsigned long int lastDebounceTime;
  unsigned long int debounceDelay;

};

#endif