#ifndef SEQUENCE_TRACKER_H
#define SEQUENCE_TRACKER_H

#include <stdint.h>

// Urmarirea numarului de ordine: de aici afla sclavul daca a pierdut pachete
// pe drum (salt in SEQ) sau daca masterul a retransmis dupa o expirare (acelasi
// SEQ a doua oara). Aritmetica pe uint8_t face trecerea 255 -> 0 fireasca.
enum SequenceStatus : uint8_t {
    SEQUENCE_FIRST,
    SEQUENCE_IN_ORDER,
    SEQUENCE_GAP,
    SEQUENCE_DUPLICATE
};

struct SequenceTracker {
    uint8_t lastSequence;
    bool hasSeenPacket;
};

void sequenceTrackerReset(SequenceTracker *tracker);

// lostCountOut primeste numarul de pachete lipsa cand rezultatul este
// SEQUENCE_GAP si 0 in celelalte cazuri.
SequenceStatus sequenceTrackerAccept(SequenceTracker *tracker, uint8_t sequence,
                                     uint8_t *lostCountOut);

#endif
