#include <Arduino.h>
#include "Button.h"
#include "ledClass.h"
Led ledV(13);
Button buttonV(12);
void setup() {
  buttonV.begin();
}

void loop() {
  buttonV.update();
  if (buttonV.wasPressed())
  {
    ledV.toggle();
  }
  
}

