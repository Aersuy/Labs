#include "encoderSensor.h"

#include <Arduino_FreeRTOS.h>
#include <task.h>

// One encoder, one interrupt vector: attachInterrupt takes a plain function
// pointer, so the counters are class statics rather than instance members.
volatile uint16_t EncoderSensor::pulseCount = 0;
volatile uint32_t EncoderSensor::lastEdgeMicros = 0;

EncoderSensor::EncoderSensor(int pin)
{
    this->pin = pin;
    this->previousCount = 0;
    this->previousEdgeMicros = 0;
}

// Counts and timestamps, nothing else: no FreeRTOS call, no arithmetic the
// task could do instead. micros() is safe here because it reads Timer0 state
// without needing interrupts enabled.
void EncoderSensor::handleEdge()
{
    uint32_t now = micros();

    if (now - EncoderSensor::lastEdgeMicros < EncoderSensor::MIN_PULSE_MICROS) return;

    EncoderSensor::lastEdgeMicros = now;
    EncoderSensor::pulseCount++;
}

void EncoderSensor::begin()
{
    // The sensor board carries its own pull-up on the LM393 output, so the
    // input stays passive here.
    pinMode(this->pin, INPUT);

    EncoderSensor::pulseCount = 0;
    EncoderSensor::lastEdgeMicros = micros();

    this->previousCount = 0;
    this->previousEdgeMicros = EncoderSensor::lastEdgeMicros;

    attachInterrupt(digitalPinToInterrupt(this->pin), EncoderSensor::handleEdge, RISING);
}

EncoderDelta EncoderSensor::read()
{
    uint16_t count;
    uint32_t edgeMicros;

    taskENTER_CRITICAL();
    count = EncoderSensor::pulseCount;
    edgeMicros = EncoderSensor::lastEdgeMicros;
    taskEXIT_CRITICAL();

    uint32_t now = micros();

    EncoderDelta delta;

    // Unsigned subtraction throughout: micros() wraps every 71 minutes and the
    // counter every 65535 pulses, and the difference stays correct across both
    // as long as nothing widens or signs these types.
    delta.pulses = (uint16_t)(count - this->previousCount);
    delta.elapsedMicros = edgeMicros - this->previousEdgeMicros;
    delta.sinceLastEdgeMicros = now - edgeMicros;

    this->previousCount = count;
    this->previousEdgeMicros = edgeMicros;
    return delta;
}
