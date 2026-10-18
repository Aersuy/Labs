#ifndef KY023_H
#define KY023_H
#include <Arduino.h>

class ky023
{
private:
    int pinX;
    int pinY;
    int pinSW;
public:
    ky023(int pinX, int pinY, int pinSW);
    void ky023Init();
    int readRawX();
    int readRawY();
    float rawToPercent(float raw);
    bool isPressed();
};

#endif
