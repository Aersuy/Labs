#ifndef DECITEMPERATURE_H
#define DECITEMPERATURE_H

#include <stdint.h>
#include <stddef.h>

// because of the perculiarities of arduino, store the temp as ints of 0.1
const int16_t DECI_PER_UNIT = 10;

// Smallest buffer formatDeciCelsius() accepts: "-3276.8" plus terminator.
const uint8_t DECI_TEXT_MIN_LEN = 8;

// string -> deci, does not touch *out for malformed inputs
bool parseDeciCelsius(const char *text, int16_t *out);

// the reverse, makes a string from a values
void formatDeciCelsius(int16_t value, char *out, uint8_t len);

#endif