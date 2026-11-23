#include "echoDistance.h"

// Viteza sunetului la 20 C este 343 m/s, adica 0,343 mm/us. Drumul masurat de
// senzor este dus-intors, deci distanta este jumatate: 0,1715 mm/us, scrisa ca
// fractie pentru ca pe AVR nu se foloseste virgula mobila (printf-ul nu are
// %f fara flaguri suplimentare de linker).
static constexpr uint32_t MICROS_TO_MM_NUMERATOR = 1715;
static constexpr uint32_t MICROS_TO_MM_DENOMINATOR = 10000;

uint16_t echoMicrosToMillimeters(uint32_t echoMicros)
{
    return (uint16_t)((echoMicros * MICROS_TO_MM_NUMERATOR) / MICROS_TO_MM_DENOMINATOR);
}

DistanceReading echoMicrosToReading(uint32_t echoMicros)
{
    // pulseIn() intoarce 0 cand expira asteptarea, deci lipsa ecoului si
    // depasirea ferestrei sunt acelasi caz pentru apelant.
    if (echoMicros == 0 || echoMicros >= ECHO_TIMEOUT_US)
    {
        return { READING_TIMEOUT, 0 };
    }

    uint16_t distanceMm = echoMicrosToMillimeters(echoMicros);
    if (distanceMm < HCSR04_MIN_MM || distanceMm > HCSR04_MAX_MM)
    {
        return { READING_OUT_OF_RANGE, 0 };
    }

    return { READING_OK, distanceMm };
}

bool isKnownReadingStatus(uint8_t rawStatus)
{
    return rawStatus <= (uint8_t)READING_OUT_OF_RANGE;
}

const char *readingStatusName(ReadingStatus status)
{
    switch (status)
    {
        case READING_NOT_YET_MEASURED: return "nemasurat";
        case READING_OK:               return "ok";
        case READING_TIMEOUT:          return "fara ecou";
        case READING_OUT_OF_RANGE:     return "in afara domeniului";
    }
    return "stare necunoscuta";
}
