#include "conditioner.h"

float Conditioner::condition(int *samples, size_t n)
{
    float weightSum = 0;
    float valueSum = 0;
    for (size_t i = 0; i < n; i++)
    {
        float weight = (float)(i + 1);   // the later values weigh more
        valueSum += samples[i] * weight;
        weightSum += weight;
    }
    return valueSum / weightSum;
}