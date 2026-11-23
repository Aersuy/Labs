#include "hcsr04.h"

#include "echoDistance.h"

// Fisa de catalog cere un impuls de declansare de minim 10 us.
static constexpr unsigned int TRIGGER_PULSE_US = 10;

Hcsr04::Hcsr04(uint8_t triggerPin, uint8_t echoPin)
    : triggerPin(triggerPin), echoPin(echoPin)
{
}

void Hcsr04::begin()
{
    pinMode(triggerPin, OUTPUT);
    pinMode(echoPin, INPUT);
    digitalWrite(triggerPin, LOW);
}

// pulseIn() este o bucla de interogare, deci un ecou lipsa tine taskul ocupat
// toata fereastra de asteptare (~25 ms). Aici este acceptabil: taskul de
// achizitie nu are termen strict, iar tactul FreeRTOS (intrerupere WDT)
// continua sa curga. Daca ar apare un task care nu suporta intarzierea, se
// trece pe captura pe intrerupere de pin, cu aceeasi conversie in aval.
uint32_t Hcsr04::measureEchoMicros()
{
    digitalWrite(triggerPin, HIGH);
    delayMicroseconds(TRIGGER_PULSE_US);
    digitalWrite(triggerPin, LOW);

    return pulseIn(echoPin, HIGH, ECHO_TIMEOUT_US);
}
