#include "keypadClass.h"
//Initialising static keypad keys
char Keypad4x4::keysMap[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
//Constructor for the keypad class
//Creates the keypad object from the Keypad class
Keypad4x4::Keypad4x4(byte rowPins[4],byte colPins[4])
{ 
  this->keypadInstance = new Keypad(makeKeymap(keysMap),&rowPins[0],&colPins[0],4,4);
}
//Gets the currently pressed key
char Keypad4x4::getKey()
{
  return this->keypadInstance->getKey();
}