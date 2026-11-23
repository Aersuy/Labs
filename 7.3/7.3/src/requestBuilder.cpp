#include "requestBuilder.h"

#include "packetPayload.h"

BinaryPacket buildRequest(uint8_t command, uint8_t sequence, bool isLedOn)
{
    BinaryPacket request;
    request.deviceId = DEVICE_ID_SLAVE;
    request.sequence = sequence;
    request.command = command;
    request.payloadLength = 0;

    if (command == CMD_SET_LED)
    {
        request.payloadLength = LED_PAYLOAD_LEN;
        packLedRequest(isLedOn, request.payload);
    }
    return request;
}

bool isResponseToRequest(const BinaryPacket *response, const BinaryPacket *request)
{
    if (response->deviceId != request->deviceId) return false;
    return response->sequence == request->sequence;
}
