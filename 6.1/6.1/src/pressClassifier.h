#ifndef PRESS_CLASSIFIER_H
#define PRESS_CLASSIFIER_H

#include <stdint.h>

// Felul apasarii, decis abia cand fereastra de dubla apasare s-a inchis.
enum PressKind : uint8_t {
    PRESS_NONE = 0,
    PRESS_SINGLE,
    PRESS_DOUBLE
};

// Memoria clasificatorului intre doua interogari ale butonului.
struct PressClassifier {
    bool isWaitingSecondPress;
    uint16_t ticksSinceFirstPress;
};

struct PressClassification {
    PressClassifier next;
    PressKind kind;
};

// does not care about the physical button
PressClassification classifyPressTick(PressClassifier current,
                                       bool isPressEdge,
                                       uint16_t windowTicks);

#endif