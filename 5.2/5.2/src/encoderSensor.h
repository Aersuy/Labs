#ifndef ENCODERSENSOR_H
#define ENCODERSENSOR_H

#include <Arduino.h>

#include "speedScale.h"

// The only module that touches the interrupt. All conversion lives in
// speedScale, so the maths stays deterministic and testable on the host.
//
// The WYGH206 slot sensor is a single channel: it reports that the shaft
// moved, never which way. Direction is therefore not observable here, and the
// application has to keep the drive one-way for the measurement to mean what
// the controller assumes it means.
class EncoderSensor {
    public:
    explicit EncoderSensor(int pin);

    // Attaches the interrupt. The pin must be interrupt-capable: on the Mega
    // that is 2, 3, 18, 19, 20 or 21 - NOT 6, which has neither an external
    // interrupt nor a pin-change one (those cover ports B, J and K only).
    void begin();

    // Pulses and elapsed time since the previous call. Both are read in one
    // critical section: the count and the timestamp of the edge that produced
    // it are a pair, and sampling them separately tears the measurement
    // whenever an edge lands between the two reads.
    EncoderDelta read();

    // Faster than this is not a pulse but brush noise coupling into the
    // sensor line: 1 ms between edges is 3000 RPM at 20 pulses per
    // revolution, an order of magnitude past what the gearbox can do.
    static const uint32_t MIN_PULSE_MICROS = 1000UL;

    private:
    static void handleEdge();

    static volatile uint16_t pulseCount;
    static volatile uint32_t lastEdgeMicros;

    int pin;
    uint16_t previousCount;
    uint32_t previousEdgeMicros;
};

#endif
