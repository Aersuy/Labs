#include "packetCodec.h"

static uint8_t xorBytes(const uint8_t *bytes, uint8_t count)
{
    uint8_t accumulator = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        accumulator ^= bytes[i];
    }
    return accumulator;
}

uint8_t packetChecksum(const uint8_t *frame, uint8_t payloadLength)
{
    uint8_t coveredCount = (uint8_t)(payloadLength + PACKET_HEADER_FIELDS);
    return xorBytes(frame + PACKET_ID_OFFSET, coveredCount);
}

static void copyBytes(const uint8_t *source, uint8_t count, uint8_t *destination)
{
    for (uint8_t i = 0; i < count; i++)
    {
        destination[i] = source[i];
    }
}

static void writeHeader(const BinaryPacket *packet, uint8_t *frameOut)
{
    frameOut[PACKET_START_OFFSET] = PACKET_START;
    frameOut[PACKET_ID_OFFSET] = packet->deviceId;
    frameOut[PACKET_SEQ_OFFSET] = packet->sequence;
    frameOut[PACKET_CMD_OFFSET] = packet->command;
    frameOut[PACKET_LEN_OFFSET] = packet->payloadLength;
}

uint8_t encodePacket(const BinaryPacket *packet, uint8_t *frameOut)
{
    uint8_t payloadLength = packet->payloadLength;
    if (payloadLength > PACKET_MAX_PAYLOAD) return 0;

    writeHeader(packet, frameOut);
    copyBytes(packet->payload, payloadLength, frameOut + PACKET_PAYLOAD_OFFSET);

    uint8_t checksumOffset = (uint8_t)(PACKET_PAYLOAD_OFFSET + payloadLength);
    frameOut[checksumOffset] = packetChecksum(frameOut, payloadLength);
    frameOut[checksumOffset + 1] = PACKET_END;

    return (uint8_t)(payloadLength + PACKET_OVERHEAD);
}

const char *commandName(uint8_t command)
{
    switch (command)
    {
        case CMD_PING:      return "PING";
        case CMD_READ_TEMP: return "TEMP";
        case CMD_SET_LED:   return "LED";
        case CMD_GET_STATS: return "STATS";
        case RESP_ACK:      return "ACK";
        case RESP_DATA:     return "DATA";
        case RESP_NACK:     return "NACK";
    }
    return "?";
}
