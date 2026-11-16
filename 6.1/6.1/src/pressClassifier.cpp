#include "pressClassifier.h"

static PressClassification onPressEdge(PressClassifier current)
{
    PressClassification result;
    result.next.ticksSinceFirstPress = 0;

    if (current.isWaitingSecondPress)
    {
        result.next.isWaitingSecondPress = false;
        result.kind = PRESS_DOUBLE;
        return result;
    }

    result.next.isWaitingSecondPress = true;
    result.kind = PRESS_NONE;
    return result;
}

static PressClassification onIdleTick(PressClassifier current,
                                       uint16_t windowTicks)
{
    PressClassification result;
    result.next = current;
    result.kind = PRESS_NONE;

    if (!current.isWaitingSecondPress) return result;

    result.next.ticksSinceFirstPress++;
    if (result.next.ticksSinceFirstPress < windowTicks) return result;

    result.next.isWaitingSecondPress = false;
    result.next.ticksSinceFirstPress = 0;
    result.kind = PRESS_SINGLE;
    return result;
}

PressClassification classifyPressTick(PressClassifier current,
                                       bool isPressEdge,
                                       uint16_t windowTicks)
{
    if (isPressEdge) return onPressEdge(current);
    return onIdleTick(current, windowTicks);
}