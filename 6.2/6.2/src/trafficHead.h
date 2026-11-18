#ifndef TRAFFIC_HEAD_H
#define TRAFFIC_HEAD_H

#include "ledClass.h"
#include "trafficFsm.h"

// Un cap de semafor: trei LED-uri comandate impreuna, din care cel mult unul
// este aprins. Compune driverul Led, nu il modifica.
class TrafficHead
{
  public:
    TrafficHead(int redPin, int yellowPin, int greenPin);
    void begin();
    void show(LampColor color);
  private:
    Led red;
    Led yellow;
    Led green;
};

#endif
