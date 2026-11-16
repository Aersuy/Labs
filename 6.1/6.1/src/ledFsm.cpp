#include "ledFsm.h"

// S0 OFF
static LedFsmState nextFromOff(LedFsmEvent event)
{
    switch (event)
    {
        case FSM_EVENT_SHORT_PRESS:  return FSM_ON;
        case FSM_EVENT_DOUBLE_PRESS: return FSM_BLINK;
        case FSM_EVENT_INACTIVITY:   return FSM_OFF;
    }
    return FSM_OFF;
}

// S1 ON
static LedFsmState nextFromOn(LedFsmEvent event)
{
    switch (event)
    {
        case FSM_EVENT_SHORT_PRESS:  return FSM_OFF;
        case FSM_EVENT_DOUBLE_PRESS: return FSM_BLINK;
        case FSM_EVENT_INACTIVITY:   return FSM_TIMEOUT_WARNING;
   }
   return FSM_ON;
}


//S2 BLINK
static LedFsmState nextFromBlink(LedFsmEvent event)
{
    switch (event)
    {
        case FSM_EVENT_SHORT_PRESS:  return FSM_OFF;
        case FSM_EVENT_DOUBLE_PRESS: return FSM_ON;
        case FSM_EVENT_INACTIVITY:   return FSM_BLINK;
    }
    return FSM_BLINK;
}

// S3 TIMEOUT_WARNING: orice apasare anuleaza stingerea automata.
static LedFsmState nextFromWarning(LedFsmEvent event)
{
    switch (event)
    {
        case FSM_EVENT_SHORT_PRESS:  return FSM_ON;
        case FSM_EVENT_DOUBLE_PRESS: return FSM_BLINK;
        case FSM_EVENT_INACTIVITY:   return FSM_OFF;
    }
    return FSM_OFF;
}

LedFsmState ledFsmNext(LedFsmState current, LedFsmEvent event)
{
    switch (current)
    {
        case FSM_OFF:             return nextFromOff(event);
        case FSM_ON:              return nextFromOn(event);
        case FSM_BLINK:           return nextFromBlink(event);
        case FSM_TIMEOUT_WARNING: return nextFromWarning(event);
    }
    return FSM_OFF;
}

LedPattern ledFsmPattern(LedFsmState state)
{
    LedPattern pattern = { false, false, 0 };

    switch (state)
    {
        case FSM_OFF:
            break;
        case FSM_ON:
            pattern.isOn = true;
            break;
        case FSM_BLINK:
            pattern.isBlinking = true;
            pattern.halfPeriodMs = BLINK_PERIOD_MS / 2;
            break;
        case FSM_TIMEOUT_WARNING:
            pattern.isBlinking = true;
            pattern.halfPeriodMs = WARNING_PERIOD_MS / 2;
            break;
    }
    return pattern;
}

uint16_t ledFsmTimeoutMs(LedFsmState state)
{
    switch (state)
    {
        case FSM_ON:              return ON_INACTIVITY_MS;
        case FSM_TIMEOUT_WARNING: return WARNING_TIMEOUT_MS;
        case FSM_OFF:
        case FSM_BLINK:           return 0;
    }
    return 0;
}

const char *ledFsmStateName(LedFsmState state)
{
    switch (state)
    {
        case FSM_OFF:             return "S0 OFF";
        case FSM_ON:              return "S1 ON";
        case FSM_BLINK:           return "S2 BLINK";
        case FSM_TIMEOUT_WARNING: return "S3 TIMEOUT_WARNING";
    }
    return "?";
}

const char *ledFsmEventName(LedFsmEvent event)
{
    switch (event)
    {
        case FSM_EVENT_SHORT_PRESS:  return "apasare simpla";
        case FSM_EVENT_DOUBLE_PRESS: return "apasare dubla";
        case FSM_EVENT_INACTIVITY:   return "inactivitate";
    }
    return "?";
}

const char *ledFsmPatternName(LedFsmState state)
{
    switch (state)
    {
        case FSM_OFF:             return "stins";
        case FSM_ON:              return "aprins continuu";
        case FSM_BLINK:           return "clipire lenta";
        case FSM_TIMEOUT_WARNING: return "clipire rapida";
    }
    return "?";
}