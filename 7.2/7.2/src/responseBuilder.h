#ifndef RESPONSE_BUILDER_H
#define RESPONSE_BUILDER_H

#include <stdint.h>

#include "binaryPacket.h"
#include "packetPayload.h"

// Ce poate cere un pachet de la iesirea fizica. Constructorul nu atinge LED-ul:
// decide, iar taskul care il apeleaza aplica. Asa decizia ramane determinista
// si testabila pe calculator.
enum LedAction : uint8_t {
    LED_ACTION_NONE,
    LED_ACTION_TURN_OFF,
    LED_ACTION_TURN_ON
};

// Tot ce are nevoie sclavul ca sa raspunda, adunat de apelant: temperatura si
// contoarele vin ca argumente, nu sunt citite aici.
struct SlaveContext {
    uint8_t deviceId;
    int16_t temperatureDeci;
    SlaveStats stats;
};

struct SlaveResponse {
    BinaryPacket packet;
    LedAction ledAction;
};

SlaveResponse buildResponse(const BinaryPacket *request, const SlaveContext *context);

// Raspunsul dat cand FIFO-ul este plin: pachetul a fost valid, dar nu mai are
// unde sa fie pus, deci masterul afla motivul in loc sa astepte degeaba.
SlaveResponse buildFifoFullNack(const BinaryPacket *request, uint8_t deviceId);

#endif
