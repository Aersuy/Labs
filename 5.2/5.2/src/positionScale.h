#ifndef POSITIONSCALE_H
#define POSITIONSCALE_H

#include <stdint.h>

// 10-bit AVR ADC.
const int16_t ADC_RAW_MIN = 0;
const int16_t ADC_RAW_MAX = 1023;

// Mechanical travel of the feedback potentiometer, in tenths of a degree.
// Positions, set points and errors are int16_t deci-degrees everywhere, the
// same fixed-point convention lab 5.1 introduced for temperatures: AVR printf
// has no %f and float maths costs too much inside a 50 Hz loop.
const int16_t POSITION_MIN_DECI_DEG = 0;
const int16_t POSITION_MAX_DECI_DEG = 2700;   // 270-degree potentiometer

// How close to a rail the ADC must sit before the wiper counts as pegged.
const int16_t ADC_LIMIT_MARGIN = 5;

typedef enum {
    POSITION_READ_OK = 0,
    // ADC pegged at a rail. On a single channel the end of travel and a broken
    // wiper are indistinguishable, so this reports the fact rather than
    // pretending to tell them apart; either way, pushing harder is wrong.
    POSITION_READ_AT_LIMIT
} PositionReadStatus;

const char *positionReadStatusName(PositionReadStatus status);

typedef struct {
    int16_t positionDeciDeg;
    PositionReadStatus status;
} PositionSample;

int16_t rawToPositionDeciDeg(int16_t raw);
bool isRawAtLimit(int16_t raw);
PositionSample buildPositionSample(int16_t raw);

#endif