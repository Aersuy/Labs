#include "pidControlState.h"

// Writers overwrite, readers never block: a stale snapshot is always better
// than a report task that stalls the control loop.
void pidControlStatePublish(QueueHandle_t mailbox, const PidControlState *state)
{
    xQueueOverwrite(mailbox, state);
}

bool pidControlStateRead(QueueHandle_t mailbox, PidControlState *out)
{
    return xQueuePeek(mailbox, out, 0) == pdPASS;
}