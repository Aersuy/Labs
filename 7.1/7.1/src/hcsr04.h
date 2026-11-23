#ifndef HCSR04_H
#define HCSR04_H

#include <Arduino.h>

// Singurul modul al sclavului legat de pini si de timp real. Nu converteste
// microsecundele in milimetri: conversia sta in echoDistance, ca sa poata fi
// testata pe calculator.
class Hcsr04
{
  public:
    Hcsr04(uint8_t triggerPin, uint8_t echoPin);
    void begin();

    // Durata bruta a ecoului, in microsecunde. 0 inseamna ca fereastra de
    // asteptare a expirat (ECHO_TIMEOUT_US din echoDistance.h).
    uint32_t measureEchoMicros();

  private:
    uint8_t triggerPin;
    uint8_t echoPin;
};

#endif
