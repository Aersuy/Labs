#include "packetPayload.h"

constexpr uint8_t LED_OFF_VALUE = 0;
constexpr uint8_t LED_ON_VALUE = 1;

void packTemperature(int16_t deciCelsius, uint8_t *payloadOut)
{
    uint16_t raw = (uint16_t)deciCelsius;
    payloadOut[0] = (uint8_t)(raw >> 8);
    payloadOut[1] = (uint8_t)(raw & 0xFF);
}

bool unpackTemperature(const uint8_t *payload, uint8_t payloadLength, int16_t *deciCelsiusOut)
{
    if (payloadLength != TEMPERATURE_PAYLOAD_LEN) return false;

    uint16_t raw = (uint16_t)(((uint16_t)payload[0] << 8) | payload[1]);
    *deciCelsiusOut = (int16_t)raw;
    return true;
}

void packLedRequest(bool isOn, uint8_t *payloadOut)
{
    payloadOut[0] = isOn ? LED_ON_VALUE : LED_OFF_VALUE;
}

// Orice alta valoare decat 0 sau 1 este o cerere invalida, nu un "aprinde":
// un octet corupt care a trecut suma de control nu are voie sa comande iesirea.
bool unpackLedRequest(const uint8_t *payload, uint8_t payloadLength, bool *isOnOut)
{
    if (payloadLength != LED_PAYLOAD_LEN) return false;
    if (payload[0] > LED_ON_VALUE) return false;

    *isOnOut = (payload[0] == LED_ON_VALUE);
    return true;
}

void packStats(const SlaveStats *stats, uint8_t *payloadOut)
{
    payloadOut[0] = stats->acceptedCount;
    payloadOut[1] = stats->fifoDroppedCount;
    payloadOut[2] = stats->lostCount;
    payloadOut[3] = stats->corruptedCount;
}

bool unpackStats(const uint8_t *payload, uint8_t payloadLength, SlaveStats *statsOut)
{
    if (payloadLength != STATS_PAYLOAD_LEN) return false;

    statsOut->acceptedCount = payload[0];
    statsOut->fifoDroppedCount = payload[1];
    statsOut->lostCount = payload[2];
    statsOut->corruptedCount = payload[3];
    return true;
}
