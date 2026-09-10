#ifndef KEYPADCLASS_H
#define KEYPADCLASS_H

#include <Arduino.h>
#include <Keypad.h>

class Keypad4x4 {
  public:
    Keypad4x4(byte rowPins[4],byte colPins[4]);
    char getKey();

  private:
    Keypad* keypadInstance;
    static char keysMap[4][4];
    
    
};

#endif