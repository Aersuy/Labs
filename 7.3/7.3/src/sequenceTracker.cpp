#include "sequenceTracker.h"

// Jumatatea de sus a intervalului inseamna "in urma": un SEQ mai mic cu pana
// la 128 este un pachet vechi, nu un salt inainte de 128 pachete.
constexpr uint8_t SEQUENCE_FORWARD_LIMIT = 127;

void sequenceTrackerReset(SequenceTracker *tracker)
{
    tracker->lastSequence = 0;
    tracker->hasSeenPacket = false;
}

static SequenceStatus acceptFirst(SequenceTracker *tracker, uint8_t sequence)
{
    tracker->lastSequence = sequence;
    tracker->hasSeenPacket = true;
    return SEQUENCE_FIRST;
}

SequenceStatus sequenceTrackerAccept(SequenceTracker *tracker, uint8_t sequence,
                                     uint8_t *lostCountOut)
{
    *lostCountOut = 0;
    if (!tracker->hasSeenPacket) return acceptFirst(tracker, sequence);

    uint8_t distance = (uint8_t)(sequence - tracker->lastSequence);
    if (distance == 0) return SEQUENCE_DUPLICATE;
    if (distance > SEQUENCE_FORWARD_LIMIT) return SEQUENCE_DUPLICATE;

    tracker->lastSequence = sequence;
    if (distance == 1) return SEQUENCE_IN_ORDER;

    *lostCountOut = (uint8_t)(distance - 1);
    return SEQUENCE_GAP;
}
