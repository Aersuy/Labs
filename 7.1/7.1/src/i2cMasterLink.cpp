#include "i2cMasterLink.h"

#include <Wire.h>

#include "i2cProtocol.h"

void i2cMasterLinkBegin()
{
    Wire.begin();
}

static bool sendReadCommand()
{
    Wire.beginTransmission(SLAVE_I2C_ADDRESS);
    Wire.write(CMD_READ_DISTANCES);
    return Wire.endTransmission() == 0;
}

// Octetii in plus se consuma: Wire pastreaza tamponul intre transferuri, iar un
// rest necitit ar deplasa cadrul urmator.
static uint8_t readAnswer(uint8_t *frameOut, uint8_t expectedLen)
{
    Wire.requestFrom(SLAVE_I2C_ADDRESS, expectedLen);

    uint8_t storedLen = 0;
    while (Wire.available())
    {
        uint8_t received = (uint8_t)Wire.read();
        if (storedLen < expectedLen) frameOut[storedLen++] = received;
    }
    return storedLen;
}

QueryStatus queryDistanceFrame(uint8_t *frameOut, uint8_t expectedLen,
                               uint8_t *receivedLenOut)
{
    *receivedLenOut = 0;
    if (!sendReadCommand()) return QUERY_COMMAND_NOT_ACKED;

    uint8_t receivedLen = readAnswer(frameOut, expectedLen);
    *receivedLenOut = receivedLen;

    if (receivedLen < expectedLen) return QUERY_SHORT_ANSWER;
    return QUERY_OK;
}

const char *queryStatusName(QueryStatus status)
{
    switch (status)
    {
        case QUERY_OK:                return "ok";
        case QUERY_COMMAND_NOT_ACKED: return "sclavul nu raspunde";
        case QUERY_SHORT_ANSWER:      return "raspuns prea scurt";
    }
    return "stare necunoscuta";
}
