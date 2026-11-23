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
    if (report->sensorCount > SENSOR_COUNT) return 0;

    uint8_t *cursor = payloadOut;
    *cursor++ = report->sequence;
    *cursor++ = report->sensorCount;

    for (uint8_t i = 0; i < report->sensorCount; i++)
    {
        cursor = packReading(&report->readings[i], cursor);
    }
    return (uint8_t)(cursor - payloadOut);
}

static const uint8_t *unpackReading(const uint8_t *cursor, DistanceReading *readingOut)
{
    readingOut->status = (ReadingStatus)*cursor++;
    uint8_t lowByte = *cursor++;
    uint8_t highByte = *cursor++;
    readingOut->distanceMm = (uint16_t)lowByte | ((uint16_t)highByte << 8);
    return cursor;
}

static bool hasKnownStatuses(const uint8_t *readingBytes, uint8_t sensorCount)
{
    for (uint8_t i = 0; i < sensorCount; i++)
    {
        if (!isKnownReadingStatus(readingBytes[i * BYTES_PER_READING])) return false;
    }
    return true;
}

static bool isPayloadWellFormed(const uint8_t *payload, uint8_t payloadLen)
{
    if (payloadLen < PAYLOAD_HEADER_BYTES) return false;

    uint8_t sensorCount = payload[1];
    if (sensorCount > SENSOR_COUNT) return false;
    if (payloadLen < (uint8_t)(PAYLOAD_HEADER_BYTES + sensorCount * BYTES_PER_READING))
    {
        return false;
    }
    return hasKnownStatuses(payload + PAYLOAD_HEADER_BYTES, sensorCount);
}

bool unpackDistancePayload(const uint8_t *payload, uint8_t payloadLen,
                           DistanceReport *reportOut)
{
    if (!isPayloadWellFormed(payload, payloadLen)) return false;

    reportOut->sequence = payload[0];
    reportOut->sensorCount = payload[1];

    const uint8_t *cursor = payload + PAYLOAD_HEADER_BYTES;
    for (uint8_t i = 0; i < reportOut->sensorCount; i++)
    {
        cursor = unpackReading(cursor, &reportOut->readings[i]);
    }
    return true;
}
