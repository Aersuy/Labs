#include "frameCodec.h"

static uint8_t xorBytes(const uint8_t *bytes, uint8_t count)
{
    uint8_t accumulator = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        accumulator ^= bytes[i];
    }
    return accumulator;
}

// Suma de control acopera si LENGTH, nu doar PAYLOAD: un LENGTH corupt ar
// schimba altfel doar unde se citeste suma, iar cadrul ar putea trece
// verificarea cu date deplasate.
uint8_t frameChecksum(const uint8_t *frame, uint8_t payloadLen)
{
    return xorBytes(frame + FRAME_LENGTH_OFFSET, (uint8_t)(payloadLen + 1));
}

static void copyBytes(const uint8_t *source, uint8_t count, uint8_t *destination)
{
    for (uint8_t i = 0; i < count; i++)
    {
        destination[i] = source[i];
    }
}

uint8_t encodeFrame(const uint8_t *payload, uint8_t payloadLen, uint8_t *frameOut)
{
    if (payloadLen > FRAME_MAX_PAYLOAD) return 0;

    frameOut[FRAME_HEAD_OFFSET] = FRAME_HEAD;
    frameOut[FRAME_LENGTH_OFFSET] = payloadLen;
    copyBytes(payload, payloadLen, frameOut + FRAME_PAYLOAD_OFFSET);
    frameOut[FRAME_PAYLOAD_OFFSET + payloadLen] = frameChecksum(frameOut, payloadLen);

    return (uint8_t)(payloadLen + FRAME_OVERHEAD);
}

FrameStatus decodeFrame(const uint8_t *frame, uint8_t frameLen,
                        uint8_t *payloadOut, uint8_t *payloadLenOut)
{
    if (frameLen < FRAME_OVERHEAD) return FRAME_TRUNCATED;
    if (frame[FRAME_HEAD_OFFSET] != FRAME_HEAD) return FRAME_BAD_HEAD;

    uint8_t payloadLen = frame[FRAME_LENGTH_OFFSET];
    if (payloadLen > FRAME_MAX_PAYLOAD) return FRAME_BAD_LENGTH;
    if (frameLen < (uint8_t)(payloadLen + FRAME_OVERHEAD)) return FRAME_TRUNCATED;

    if (frame[FRAME_PAYLOAD_OFFSET + payloadLen] != frameChecksum(frame, payloadLen))
    {
        return FRAME_BAD_CHECKSUM;
    }

    copyBytes(frame + FRAME_PAYLOAD_OFFSET, payloadLen, payloadOut);
    *payloadLenOut = payloadLen;
    return FRAME_OK;
}

const char *frameStatusName(FrameStatus status)
{
    switch (status)
    {
        case FRAME_OK:            return "ok";
        case FRAME_TRUNCATED:     return "cadru incomplet";
        case FRAME_BAD_HEAD:      return "HEAD gresit";
        case FRAME_BAD_LENGTH:    return "LENGTH gresit";
        case FRAME_BAD_CHECKSUM:  return "suma de control gresita";
    }
    return "stare necunoscuta";
}
