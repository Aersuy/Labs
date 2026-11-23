#ifndef DISTANCE_PAYLOAD_H
#define DISTANCE_PAYLOAD_H

#include <stdint.h>

#include "echoDistance.h"
#include "frameCodec.h"

// Numarul de senzori ultrasonici de pe sclav. Protocolul transmite si
// sensorCount, deci masterul nu trebuie recompilat daca se citesc mai putini
// senzori decat incap in cadru.
constexpr uint8_t SENSOR_COUNT = 2;

// PAYLOAD: sequence(1) + sensorCount(1) + per senzor status(1) + distanceMm(2)
constexpr uint8_t PAYLOAD_HEADER_BYTES = 2;
constexpr uint8_t BYTES_PER_READING = 3;
constexpr uint8_t DISTANCE_PAYLOAD_LEN =
    PAYLOAD_HEADER_BYTES + SENSOR_COUNT * BYTES_PER_READING;
constexpr uint8_t DISTANCE_FRAME_LEN = DISTANCE_PAYLOAD_LEN + FRAME_OVERHEAD;

static_assert(DISTANCE_PAYLOAD_LEN <= FRAME_MAX_PAYLOAD,
              "PAYLOAD-ul depaseste cadrul maxim admis de protocol");

// sequence creste la fiecare ciclu de achizitie, deci masterul poate deosebi
// un raspuns proaspat de aceeasi masuratoare citita a doua oara.
struct DistanceReport {
    uint8_t sequence;
    uint8_t sensorCount;
    DistanceReading readings[SENSOR_COUNT];
};

// Intoarce numarul de octeti scrisi, sau 0 daca report->sensorCount depaseste
// SENSOR_COUNT (nu se scrie nimic in payloadOut).
uint8_t packDistancePayload(const DistanceReport *report, uint8_t *payloadOut);

// Datele vin de pe magistrala, deci nu sunt de incredere: lungimea, numarul de
// senzori si fiecare octet de stare se verifica inainte de a umple structura.
bool unpackDistancePayload(const uint8_t *payload, uint8_t payloadLen,
                           DistanceReport *reportOut);

#endif
