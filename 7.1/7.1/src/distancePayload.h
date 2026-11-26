#ifndef DISTANCE_PAYLOAD_H
#define DISTANCE_PAYLOAD_H

#include <stdint.h>

#include "echoDistance.h"
#include "frameCodec.h"

// PAYLOAD: sequence(1) + status(1) + distanceMm(2)
constexpr uint8_t PAYLOAD_HEADER_BYTES = 1;
constexpr uint8_t BYTES_PER_READING = 3;
constexpr uint8_t DISTANCE_PAYLOAD_LEN = PAYLOAD_HEADER_BYTES + BYTES_PER_READING;
constexpr uint8_t DISTANCE_FRAME_LEN = DISTANCE_PAYLOAD_LEN + FRAME_OVERHEAD;

static_assert(DISTANCE_PAYLOAD_LEN <= FRAME_MAX_PAYLOAD,
              "PAYLOAD-ul depaseste cadrul maxim admis de protocol");

// sequence creste la fiecare ciclu de achizitie, deci masterul poate deosebi
// un raspuns proaspat de aceeasi masuratoare citita a doua oara.
struct DistanceReport {
    uint8_t sequence;
    DistanceReading reading;
};

// Intoarce numarul de octeti scrisi, mereu DISTANCE_PAYLOAD_LEN: cadrul are
// lungime fixa, deci ambalarea nu are cum sa eșueze.
uint8_t packDistancePayload(const DistanceReport *report, uint8_t *payloadOut);

// Datele vin de pe magistrala, deci nu sunt de incredere: lungimea si octetul
// de stare se verifica inainte de a umple structura.
bool unpackDistancePayload(const uint8_t *payload, uint8_t payloadLen,
                           DistanceReport *reportOut);

#endif
