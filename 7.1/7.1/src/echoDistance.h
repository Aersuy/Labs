#ifndef ECHO_DISTANCE_H
#define ECHO_DISTANCE_H

#include <stdint.h>

// Rezultatul unei masuratori este starea plus valoarea, nu o valoare cu
// inteles special: "nu am ecou" si "0 mm" trebuie sa se deosebeasca pe
// magistrala, altfel masterul afiseaza 0 mm pentru un senzor deconectat.
enum ReadingStatus : uint8_t {
    READING_NOT_YET_MEASURED = 0,   // starea de la pornire, inainte de primul ciclu
    READING_OK,
    READING_TIMEOUT,                // niciun ecou in fereastra de asteptare
    READING_OUT_OF_RANGE            // ecou valid, dar in afara domeniului senzorului
};

// distanceMm are inteles numai cand status == READING_OK; in celelalte cazuri
// este 0, ca sa nu existe doua reprezentari pentru acelasi "nu se stie".
struct DistanceReading {
    ReadingStatus status;
    uint16_t distanceMm;
};

// Domeniul din fisa de catalog a HC-SR04: 2 cm ... 4 m.
constexpr uint16_t HCSR04_MIN_MM = 20;
constexpr uint16_t HCSR04_MAX_MM = 4000;

// Fereastra de asteptare a ecoului, aleasa putin peste dus-intorsul pentru
// 4 m (~23,3 ms). Se foloseste si ca argument pentru pulseIn().
constexpr uint32_t ECHO_TIMEOUT_US = 25000;

DistanceReading echoMicrosToReading(uint32_t echoMicros);
uint16_t echoMicrosToMillimeters(uint32_t echoMicros);

// Validarea octetului de stare venit de pe magistrala: datele primite de la
// alt microcontroler nu sunt de incredere, deci nu se convertesc direct la
// enum fara verificare.
bool isKnownReadingStatus(uint8_t rawStatus);
const char *readingStatusName(ReadingStatus status);

#endif
