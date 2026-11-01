#include "controlState.h"

// The mailbox always holds the latest snapshot: writers overwrite, readers
// peek, so nobody ever blocks waiting for the other side.
void controlStatePublish(QueueHandle_t mailbox, const ControlState *state)
{
    xQueueOverwrite(mailbox, state);
}

bool controlStateRead(QueueHandle_t mailbox, ControlState *out)
{
    return xQueuePeek(mailbox, out, 0) == pdPASS;
}