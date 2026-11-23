#include "i2cSlaveLink.h"

#include <Wire.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>

#include "distanceBuffer.h"
#include "frameCodec.h"
#include "i2cProtocol.h"

// Ultima comanda primita si contorul de comenzi necunoscute: scrise in ISR-ul
// TWI, deci volatile.
static volatile uint8_t lastCommand = CMD_READ_DISTANCES;
static volatile uint16_t unknownCommandCount = 0;

// Raspunsul la o comanda necunoscuta: un cadru valid, dar cu PAYLOAD gol.
static uint8_t emptyFrame[FRAME_OVERHEAD];
static uint8_t emptyFrameLen = 0;

// Rulat in ISR: doar citeste comanda si o reține. Octetii in plus se consuma,
// altfel ar ramane in tampon si ar fi luati drept comanda urmatoare.
static void onCommandReceived(int byteCount)
{
    if (byteCount <= 0) return;

    uint8_t command = (uint8_t)Wire.read();
    while (Wire.available())
    {
        Wire.read();
    }

    if (command != CMD_READ_DISTANCES) unknownCommandCount++;
    lastCommand = command;
}

// Rulat in ISR: o singura copiere in tamponul TWI, fara apeluri FreeRTOS si
// fara formatarea cadrului (aceea se face in taskul de publicare).
static void onFrameRequested()
{
    if (lastCommand != CMD_READ_DISTANCES)
    {
        Wire.write(emptyFrame, emptyFrameLen);
        return;
    }

    uint8_t frameLen = 0;
    const uint8_t *frame = committedFrame(&frameLen);
    Wire.write(frame, frameLen);
}

void i2cSlaveLinkBegin()
{
    static const uint8_t noPayload[1] = { 0 };
    emptyFrameLen = encodeFrame(noPayload, 0, emptyFrame);

    Wire.begin(SLAVE_I2C_ADDRESS);
    Wire.onReceive(onCommandReceived);
    Wire.onRequest(onFrameRequested);
}

uint16_t i2cSlaveUnknownCommandCount()
{
    taskENTER_CRITICAL();
    uint16_t count = unknownCommandCount;
    taskEXIT_CRITICAL();
    return count;
}
