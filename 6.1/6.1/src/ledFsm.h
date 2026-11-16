#ifndef LED_FSM_H
#define LED_FSM_H

#include <stdint.h>

//states
enum LedFsmState : uint8_t {
    FSM_OFF = 0,
    FSM_ON,
    FSM_BLINK,
    FSM_TIMEOUT_WARNING
};

//events
enum LedFsmEvent : uint8_t {
    FSM_EVENT_SHORT_PRESS = 0,
    FSM_EVENT_DOUBLE_PRESS,
    FSM_EVENT_INACTIVITY
};

// physical led state
struct LedPattern {
    bool isOn;
    bool isBlinking;
    uint16_t halfPeriodMs;
};

constexpr uint16_t BLINK_PERIOD_MS = 500;
constexpr uint16_t WARNING_PERIOD_MS = 100;
constexpr uint16_t ON_INACTIVITY_MS = 10000;
constexpr uint16_t WARNING_TIMEOUT_MS = 3000;

LedFsmState ledFsmNext(LedFsmState current, LedFsmEvent event);
LedPattern ledFsmPattern(LedFsmState state);

// 0 = starea nu armeaza niciun cronometru de inactivitate.
uint16_t ledFsmTimeoutMs(LedFsmState state);

const char *ledFsmStateName(LedFsmState state);
const char *ledFsmEventName(LedFsmEvent event);
const char *ledFsmPatternName(LedFsmState state);
#endif