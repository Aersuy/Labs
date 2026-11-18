#include "trafficFsm.h"

// Fazele cu durata fixa (galben, degajare, verde pe Nord, verde minim pe
// Est): o cerere sosita acum nu le scurteaza, iar comutatorul de noapte
// trece peste orice faza.
static TrafficState nextInTimedPhase(TrafficState current,
                                     TrafficState afterTimeout,
                                     TrafficEvent event)
{
    switch (event)
    {
        case EVENT_PHASE_TIMEOUT: return afterTimeout;
        case EVENT_NIGHT_TOGGLE:  return NIGHT_FLASH;
        case EVENT_NORTH_REQUEST: return current;
    }
    return current;
}

// S1 EAST_GREEN_HOLD: verdele de pe Est rezista cat timp nu exista cerere,
// iar expirarea asteptarii inseamna intersectie inactiva, deci mod noapte.
static TrafficState nextFromEastGreenHold(TrafficEvent event)
{
    switch (event)
    {
        case EVENT_NORTH_REQUEST: return EAST_YELLOW;
        case EVENT_PHASE_TIMEOUT: return NIGHT_FLASH;
        case EVENT_NIGHT_TOGGLE:  return NIGHT_FLASH;
    }
    return EAST_GREEN_HOLD;
}

// S7 NIGHT_FLASH: galbenul intermitent nu expira. Iese la comutator sau la
// primul vehicul detectat, iar cererea respectiva ramane memorata.
static TrafficState nextFromNightFlash(TrafficEvent event)
{
    switch (event)
    {
        case EVENT_NORTH_REQUEST: return EAST_GREEN_MIN;
        case EVENT_NIGHT_TOGGLE:  return EAST_GREEN_MIN;
        case EVENT_PHASE_TIMEOUT: return NIGHT_FLASH;
    }
    return NIGHT_FLASH;
}

TrafficState trafficFsmNext(TrafficState current, TrafficEvent event)
{
    switch (current)
    {
        case EAST_GREEN_MIN:       return nextInTimedPhase(current, EAST_GREEN_HOLD, event);
        case EAST_GREEN_HOLD:      return nextFromEastGreenHold(event);
        case EAST_YELLOW:          return nextInTimedPhase(current, ALL_RED_BEFORE_NORTH, event);
        case ALL_RED_BEFORE_NORTH: return nextInTimedPhase(current, NORTH_GREEN, event);
        case NORTH_GREEN:          return nextInTimedPhase(current, NORTH_YELLOW, event);
        case NORTH_YELLOW:         return nextInTimedPhase(current, ALL_RED_BEFORE_EAST, event);
        case ALL_RED_BEFORE_EAST:  return nextInTimedPhase(current, EAST_GREEN_MIN, event);
        case NIGHT_FLASH:          return nextFromNightFlash(event);
    }
    return EAST_GREEN_MIN;
}

static SignalPattern steadyLamps(LampColor east, LampColor north)
{
    SignalPattern pattern = { east, north, 0 };
    return pattern;
}

static SignalPattern flashingYellowLamps()
{
    SignalPattern pattern = { LAMP_YELLOW, LAMP_YELLOW, NIGHT_FLASH_PERIOD_MS / 2 };
    return pattern;
}

SignalPattern trafficFsmPattern(TrafficState state)
{
    switch (state)
    {
        case EAST_GREEN_MIN:
        case EAST_GREEN_HOLD:      return steadyLamps(LAMP_GREEN, LAMP_RED);
        case EAST_YELLOW:          return steadyLamps(LAMP_YELLOW, LAMP_RED);
        case ALL_RED_BEFORE_NORTH:
        case ALL_RED_BEFORE_EAST:  return steadyLamps(LAMP_RED, LAMP_RED);
        case NORTH_GREEN:          return steadyLamps(LAMP_RED, LAMP_GREEN);
        case NORTH_YELLOW:         return steadyLamps(LAMP_RED, LAMP_YELLOW);
        case NIGHT_FLASH:          return flashingYellowLamps();
    }
    return steadyLamps(LAMP_RED, LAMP_RED);
}

uint16_t trafficFsmPhaseMs(TrafficState state)
{
    switch (state)
    {
        case EAST_GREEN_MIN:       return EAST_MIN_GREEN_MS;
        case EAST_GREEN_HOLD:      return EAST_HOLD_IDLE_MS;
        case EAST_YELLOW:
        case NORTH_YELLOW:         return YELLOW_MS;
        case ALL_RED_BEFORE_NORTH:
        case ALL_RED_BEFORE_EAST:  return ALL_RED_MS;
        case NORTH_GREEN:          return NORTH_GREEN_MS;
        case NIGHT_FLASH:          return 0;
    }
    return 0;
}

bool trafficFsmServesRequest(TrafficState state)
{
    return state == EAST_GREEN_HOLD;
}

const char *trafficFsmStateName(TrafficState state)
{
    switch (state)
    {
        case EAST_GREEN_MIN:       return "S0 EST_VERDE_MIN";
        case EAST_GREEN_HOLD:      return "S1 EST_VERDE_LIBER";
        case EAST_YELLOW:          return "S2 EST_GALBEN";
        case ALL_RED_BEFORE_NORTH: return "S3 TOT_ROSU->NORD";
        case NORTH_GREEN:          return "S4 NORD_VERDE";
        case NORTH_YELLOW:         return "S5 NORD_GALBEN";
        case ALL_RED_BEFORE_EAST:  return "S6 TOT_ROSU->EST";
        case NIGHT_FLASH:          return "S7 MOD_NOAPTE";
    }
    return "?";
}

const char *trafficFsmEventName(TrafficEvent event)
{
    switch (event)
    {
        case EVENT_NORTH_REQUEST: return "cerere Nord";
        case EVENT_PHASE_TIMEOUT: return "faza expirata";
        case EVENT_NIGHT_TOGGLE:  return "comutator noapte";
    }
    return "?";
}

const char *lampColorName(LampColor color)
{
    switch (color)
    {
        case LAMP_OFF:    return "stins";
        case LAMP_RED:    return "ROSU";
        case LAMP_YELLOW: return "GALBEN";
        case LAMP_GREEN:  return "VERDE";
    }
    return "?";
}
