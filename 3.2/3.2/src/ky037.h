#ifndef KY037_H
#define KY037_H
#include <Arduino.h>

class ky037
{
private:
    int pin;
public:
    ky037(int pin);
    void ky037Init();
    int readRaw();
    float rawToDb(float raw);
};
#endif