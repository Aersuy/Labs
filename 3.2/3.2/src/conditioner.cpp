#include "conditioner.h"

static int median3(int a, int b, int c)
{
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}

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

void Conditioner::saltPepperFilter(int *samples, size_t n)
 {
     if (n < 3) return;

    int prevOriginal = samples[0];
    for (size_t i = 0; i < n; i++)
    {
        int current = samples[i];
        int next = (i + 1 < n) ? samples[i + 1] : current;   // clamp last edge
        int left = (i == 0) ? current : prevOriginal;        // clamp first edge

        samples[i] = median3(left, current, next);

        prevOriginal = current;
    }
 }