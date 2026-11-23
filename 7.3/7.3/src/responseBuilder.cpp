#include "responseBuilder.h"

// SEQ se intoarce neschimbat: asa masterul potriveste raspunsul cu cererea si
// deosebeste raspunsul la retransmisie de un ecou intarziat.
static SlaveResponse emptyResponse(const BinaryPacket *request, uint8_t deviceId,
                                   uint8_t command)
{
    SlaveResponse response;
    response.packet.deviceId = deviceId;
    response.packet.sequence = request->sequence;
    response.packet.command = command;
    response.packet.payloadLength = 0;
    response.ledAction = LED_ACTION_NONE;
    return response;
}

static SlaveResponse nackResponse(const BinaryPacket *request, uint8_t deviceId,
                                  uint8_t reason)
{
    SlaveResponse response = emptyResponse(request, deviceId, RESP_NACK);
    response.packet.payloadLength = NACK_PAYLOAD_LEN;
    response.packet.payload[0] = reason;
    return response;
}

// Cererile fara date trebuie sa vina fara date: un LEN nenul pe PING inseamna
// un flux desincronizat, nu o extensie a protocolului.
static bool hasEmptyPayload(const BinaryPacket *request)
{
    return request->payloadLength == 0;
}

static SlaveResponse pingResponse(const BinaryPacket *request, const SlaveContext *context)
{
    if (!hasEmptyPayload(request)) return nackResponse(request, context->deviceId, NACK_BAD_LENGTH);
    return emptyResponse(request, context->deviceId, RESP_ACK);
}

static SlaveResponse temperatureResponse(const BinaryPacket *request,
                                         const SlaveContext *context)
{
    if (!hasEmptyPayload(request)) return nackResponse(request, context->deviceId, NACK_BAD_LENGTH);

    SlaveResponse response = emptyResponse(request, context->deviceId, RESP_DATA);
    response.packet.payloadLength = TEMPERATURE_PAYLOAD_LEN;
    packTemperature(context->temperatureDeci, response.packet.payload);
    return response;
}

static SlaveResponse ledResponse(const BinaryPacket *request, const SlaveContext *context)
{
    bool isOn = false;
    if (!unpackLedRequest(request->payload, request->payloadLength, &isOn))
    {
        return nackResponse(request, context->deviceId, NACK_BAD_LENGTH);
    }

    SlaveResponse response = emptyResponse(request, context->deviceId, RESP_ACK);
    response.packet.payloadLength = LED_PAYLOAD_LEN;
    packLedRequest(isOn, response.packet.payload);
    response.ledAction = isOn ? LED_ACTION_TURN_ON : LED_ACTION_TURN_OFF;
    return response;
}

static SlaveResponse statsResponse(const BinaryPacket *request, const SlaveContext *context)
{
    if (!hasEmptyPayload(request)) return nackResponse(request, context->deviceId, NACK_BAD_LENGTH);

    SlaveResponse response = emptyResponse(request, context->deviceId, RESP_DATA);
    response.packet.payloadLength = STATS_PAYLOAD_LEN;
    packStats(&context->stats, response.packet.payload);
    return response;
}

SlaveResponse buildResponse(const BinaryPacket *request, const SlaveContext *context)
{
    switch (request->command)
    {
        case CMD_PING:      return pingResponse(request, context);
        case CMD_READ_TEMP: return temperatureResponse(request, context);
        case CMD_SET_LED:   return ledResponse(request, context);
        case CMD_GET_STATS: return statsResponse(request, context);
        default: break;
    }
    return nackResponse(request, context->deviceId, NACK_BAD_COMMAND);
}

SlaveResponse buildFifoFullNack(const BinaryPacket *request, uint8_t deviceId)
{
    return nackResponse(request, deviceId, NACK_FIFO_FULL);
}
