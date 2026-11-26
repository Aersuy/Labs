#include "distancePayload.h"

// Little-endian pe magistrala, ca pe ambele AVR-uri: asa octetii se pot citi
// direct intr-un uint16_t pe partea masterului.
static uint8_t *packReading(const DistanceReading *reading, uint8_t *cursor)
{
    *cursor++ = (uint8_t)reading->status;
    *cursor++ = (uint8_t)(reading->distanceMm & 0xFF);
    *cursor++ = (uint8_t)(reading->distanceMm >> 8);
    return cursor;
}

uint8_t packDistancePayload(const DistanceReport *report, uint8_t *payloadOut)
{
    uint8_t *cursor = payloadOut;
    *cursor++ = report->sequence;
    cursor = packReading(&report->reading, cursor);
    return (uint8_t)(cursor - payloadOut);
}

static void unpackReading(const uint8_t *cursor, DistanceReading *readingOut)
{
    readingOut->status = (ReadingStatus)cursor[0];
    uint8_t lowByte = cursor[1];
    uint8_t highByte = cursor[2];
    readingOut->distanceMm = (uint16_t)lowByte | ((uint16_t)highByte << 8);
}

bool unpackDistancePayload(const uint8_t *payload, uint8_t payloadLen,
                           DistanceReport *reportOut)
{
    if (payloadLen < DISTANCE_PAYLOAD_LEN) return false;
    if (!isKnownReadingStatus(payload[PAYLOAD_HEADER_BYTES])) return false;

    reportOut->sequence = payload[0];
    unpackReading(payload + PAYLOAD_HEADER_BYTES, &reportOut->reading);
    return true;
}
