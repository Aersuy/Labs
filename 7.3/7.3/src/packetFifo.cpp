#include "packetFifo.h"

bool packetFifoCreate(PacketFifo *fifo, uint8_t depth)
{
    fifo->droppedCount = 0;
    fifo->queue = xQueueCreate(depth, sizeof(BinaryPacket));
    return fifo->queue != NULL;
}

// Contorul creste pana la 255 si ramane acolo: ne interesam de faptul ca exista
// pierderi, nu de numarul exact dupa ore de rulare.
static void countDroppedPacket(PacketFifo *fifo)
{
    if (fifo->droppedCount < 0xFF) fifo->droppedCount++;
}

bool packetFifoPush(PacketFifo *fifo, const BinaryPacket *packet)
{
    if (xQueueSend(fifo->queue, packet, 0) == pdPASS) return true;

    countDroppedPacket(fifo);
    return false;
}

bool packetFifoPop(PacketFifo *fifo, BinaryPacket *packetOut, TickType_t ticksToWait)
{
    return xQueueReceive(fifo->queue, packetOut, ticksToWait) == pdPASS;
}

uint8_t packetFifoDroppedCount(const PacketFifo *fifo)
{
    return fifo->droppedCount;
}
