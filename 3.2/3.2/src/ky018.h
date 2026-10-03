#ifndef KY018_H
#define KY018_H
#include <Arduino.h>

class ky018
{
private:
    int pin;
public:
    ky018(int pin);
    void ky018Init();
    int readRaw();
    float readLightPercent();
};

#endif