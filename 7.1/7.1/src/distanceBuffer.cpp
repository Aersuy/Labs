#include "distanceBuffer.h"

#include <string.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>

#include "frameCodec.h"

static uint8_t frameSlots[2][FRAME_MAX_LEN];
static uint8_t frameLengths[2] = { 0, 0 };

// Indicele slotului gata de trimis. volatile pentru ca este scris de task si
// citit de ISR, iar compilatorul nu are de unde sa stie asta.
static volatile uint8_t committedSlot = 0;

// Se scrie intai slotul liber si abia apoi se comuta indicele, intr-o secțiune
// critica: ISR-ul vede ori cadrul vechi intreg, ori pe cel nou intreg, niciodata
// unul pe jumatate rescris.
void publishFrame(const uint8_t *frame, uint8_t frameLen)
{
    if (frameLen > FRAME_MAX_LEN) return;

    uint8_t spareSlot = (uint8_t)(committedSlot ^ 1);
    memcpy(frameSlots[spareSlot], frame, frameLen);
    frameLengths[spareSlot] = frameLen;

    taskENTER_CRITICAL();
    committedSlot = spareSlot;
    taskEXIT_CRITICAL();
}

const uint8_t *committedFrame(uint8_t *frameLenOut)
{
    uint8_t slot = committedSlot;
    *frameLenOut = frameLengths[slot];
    return frameSlots[slot];
}
