#ifndef PACKET_PAYLOAD_H
#define PACKET_PAYLOAD_H

#include <stdint.h>

// Intelesul octetilor din PAYLOAD. Codecul si analizorul nu il cunosc, ca sa
// poata fi folosite pentru orice comanda noua a protocolului.
constexpr uint8_t TEMPERATURE_PAYLOAD_LEN = 2;
constexpr uint8_t LED_PAYLOAD_LEN = 1;
constexpr uint8_t STATS_PAYLOAD_LEN = 4;
constexpr uint8_t NACK_PAYLOAD_LEN = 1;

// Contoare de diagnostic ale sclavului. Satureaza la 255: ne interesam de
// faptul ca exista pierderi, nu de numarul exact dupa ore de rulare.
struct SlaveStats {
    uint8_t acceptedCount;
    uint8_t fifoDroppedCount;
    uint8_t lostCount;
    uint8_t corruptedCount;
};

// Temperatura circula in zecimi de grad, intreg cu semn, octetul inalt primul:
// AVR nu are printf cu %f, iar intregii se compara exact intre cele doua placi.
void packTemperature(int16_t deciCelsius, uint8_t *payloadOut);
bool unpackTemperature(const uint8_t *payload, uint8_t payloadLength, int16_t *deciCelsiusOut);

void packLedRequest(bool isOn, uint8_t *payloadOut);
bool unpackLedRequest(const uint8_t *payload, uint8_t payloadLength, bool *isOnOut);

void packStats(const SlaveStats *stats, uint8_t *payloadOut);
bool unpackStats(const uint8_t *payload, uint8_t payloadLength, SlaveStats *statsOut);

#endif
