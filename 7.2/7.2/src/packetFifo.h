#ifndef PACKET_FIFO_H
#define PACKET_FIFO_H

#include <Arduino_FreeRTOS.h>
#include <queue.h>

#include "binaryPacket.h"

// Coada FIFO de mesaje cu lungime fixa (sizeof(BinaryPacket)) dintre taskul de
// receptie si cel care prelucreaza. Coada FreeRTOS pastreaza ordinea sosirii si
// blocheaza consumatorul cat timp este goala, deci nu are nevoie de mutex.
//
// Pachetele care nu mai incap se numara, nu se rescriu: un pachet pierdut
// trebuie sa apara in diagnostic, iar masterul il retransmite oricum.
struct PacketFifo {
    QueueHandle_t queue;
    volatile uint8_t droppedCount;
};

bool packetFifoCreate(PacketFifo *fifo, uint8_t depth);

bool packetFifoPush(PacketFifo *fifo, const BinaryPacket *packet);

bool packetFifoPop(PacketFifo *fifo, BinaryPacket *packetOut, TickType_t ticksToWait);

uint8_t packetFifoDroppedCount(const PacketFifo *fifo);

#endif
