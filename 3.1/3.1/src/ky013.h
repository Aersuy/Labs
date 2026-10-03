#ifndef KY013_H
#define KY013_H
#include <Arduino.h>
class ky013
{
private:
    int pin;
public:
    ky013(int pin);
    void ky013Init();
    float readKelvin();
    float readCelcius();
};


#endif