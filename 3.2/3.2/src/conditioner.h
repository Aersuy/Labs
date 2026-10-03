#ifndef CONDITIONER_H
#define CONDITIONER_H
#include <Arduino.h>

class Conditioner
{
public:
    float condition(int *samples, size_t n);
};
#endif