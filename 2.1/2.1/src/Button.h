#ifndef BUTTON_CLASS_H
#define BUTTON_CLASS_H
#include <Arduino.h>
class Button
{
public:
  Button(int pin);
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
  static const unsigned long int debounceDelay = 20;

};

#endif