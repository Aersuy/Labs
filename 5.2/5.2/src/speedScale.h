#ifndef SPEEDSCALE_H
#define SPEEDSCALE_H

#include <stdint.h>

// Slots in the encoder disc on the output shaft, so one revolution of the
// wheel produces this many pulses. COUNT THE SLOTS before trusting the 20:
// the disc is a generic HC-020K-style part and 20 is the common case, not a
// guarantee. A wrong value scales every reading by a constant and no amount
// of tuning will hide it.
const uint8_t ENCODER_PULSES_PER_REV = 20;

// Travel of the control variable, in tenths of an RPM. Positions became
// speeds when the feedback moved from a potentiometer to the slot encoder,
// but the fixed-point convention is the one lab 5.1 introduced: AVR printf
// has no %f and float maths costs too much inside the loop.
const int16_t SPEED_MIN_DECI_RPM = 0;
const int16_t SPEED_MAX_DECI_RPM = 2500;    // ~250 RPM, measure yours open-loop

// No edge for this long means the shaft is not turning. It has to outlast the
// slowest pulse the gearbox can produce at the lowest useful duty, or a slow
// crawl would be reported as a stall.
const uint32_t SPEED_STALL_TIMEOUT_MICROS = 400000UL;

typedef enum {
    SPEED_READ_OK = 0,
    // No edges within the timeout. A single-channel encoder cannot tell a
    // blocked shaft from an unpowered one, so this reports the fact rather
    // than pretending to tell them apart; either way, pushing harder is wrong.
    SPEED_READ_STALLED,
    // Faster than the scale can express. Reported rather than wrapped, and
    // the value is clamped, so the controller sees a sane number.
    SPEED_READ_OVER_RANGE
} SpeedReadStatus;

const char *speedReadStatusName(SpeedReadStatus status);

typedef struct {
    int16_t speedDeciRpm;
    SpeedReadStatus status;
} SpeedSample;

// What one sampling interval saw. Both ends of elapsedMicros are real pulse
// edges, never the task's wake-up, which is what keeps the measurement free
// of the window quantisation a plain pulses-per-tick count would suffer:
// at 20 pulses per revolution and a ~96 ms window, counting alone resolves
// no better than 15 RPM.
typedef struct {
    uint16_t pulses;
    uint32_t elapsedMicros;         // between the first and last edge counted
    uint32_t sinceLastEdgeMicros;   // for the stall timeout
} EncoderDelta;

int16_t pulsesToDeciRpm(uint16_t pulses, uint32_t elapsedMicros);
SpeedSample buildSpeedSample(const EncoderDelta *delta);

#endif
