#include "deciTemperature.h"

// Keeps whole * DECI_PER_UNIT inside int16_t.
static const int16_t WHOLE_MAX = 3200;
static const char DECIMAL_SEPARATOR = '.';
static const char MINUS_SIGN = '-';
static const uint8_t UNSIGNED_DIGITS_MAX = 6;

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

// Consumes consecutive digits, advancing the cursor. Fails when there is no
// digit at all or the value would overflow.
static bool readWholePart(const char **cursor, int16_t *whole)
{
    const char *reader = *cursor;
    int32_t value = 0;

    if (!isDigit(*reader)) return false;

    while (isDigit(*reader))
    {
        value = value * 10 + (*reader - '0');
        if (value > WHOLE_MAX) return false;
        reader++;
    }
    *cursor = reader;
    *whole = (int16_t)value;
    return true;
}

// Consumes ".d" and requires the string to end there; an absent fraction is 0.
static bool readFractionPart(const char **cursor, int16_t *tenths)
{
    const char *reader = *cursor;

    if (*reader == '\0')
    {
        *tenths = 0;
        return true;
    }
    if (*reader != DECIMAL_SEPARATOR) return false;

    reader++;
    if (!isDigit(*reader)) return false;

    *tenths = (int16_t)(*reader - '0');
    reader++;
    if (*reader != '\0') return false;   // "1.23" is rejected on purpose

    *cursor = reader;
    return true;
}

bool parseDeciCelsius(const char *text, int16_t *out)
{
    if (text == NULL || out == NULL) return false;

    const char *cursor = text;
    bool isNegative = (*cursor == MINUS_SIGN);
    if (isNegative) cursor++;

    int16_t whole = 0;
    int16_t tenths = 0;
    if (!readWholePart(&cursor, &whole)) return false;
    if (!readFractionPart(&cursor, &tenths)) return false;

    int16_t magnitude = (int16_t)(whole * DECI_PER_UNIT + tenths);
    *out = isNegative ? (int16_t)(-magnitude) : magnitude;
    return true;
}

// Writes the digits of value without padding; returns how many it wrote.
static uint8_t writeUnsigned(char *out, uint16_t value)
{
    char reversed[UNSIGNED_DIGITS_MAX];
    uint8_t count = 0;

    do
    {
        reversed[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value > 0);

    for (uint8_t i = 0; i < count; i++)
    {
        out[i] = reversed[count - 1 - i];
    }
    return count;
}


void formatDeciCelsius(int16_t value, char *out, uint8_t len)
{
    if (out == NULL || len == 0) return;
    if (len < DECI_TEXT_MIN_LEN)
    {
        out[0] = '\0';
        return;
    }

    bool isNegative = value < 0;
    int16_t magnitude = isNegative ? (int16_t)(-value) : value;
    uint8_t index = 0;

    if (isNegative) out[index++] = MINUS_SIGN;
    index += writeUnsigned(out + index, (uint16_t)(magnitude / DECI_PER_UNIT));
    out[index++] = DECIMAL_SEPARATOR;
    out[index++] = (char)('0' + magnitude % DECI_PER_UNIT);
    out[index] = '\0';
}