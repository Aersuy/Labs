#ifndef MILLIGAIN_H
#define MILLIGAIN_H

#include <stdint.h>
#include <stddef.h>

// Gains are stored as int16_t thousandths, the same trick deciTemperature uses
// for tenths. A separate module because gains need three decimals ("ki 0.050")
// while positions need one, and the accepted ranges differ. Rule of Three: two
// scales is not yet enough duplication to justify a generic parser.
const int16_t MILLI_PER_UNIT = 1000;

// Smallest buffer formatMilliGain() accepts: "32.767" plus terminator.
const uint8_t MILLI_TEXT_MIN_LEN = 7;

// "2.5" -> 2500, "0.05" -> 50, "20" -> 20000. Gains are never negative, so a
// leading minus is rejected. Does not touch *out for malformed input.
bool parseMilliGain(const char *text, int16_t *out);

void formatMilliGain(int16_t value, char *out, uint8_t len);

#endif