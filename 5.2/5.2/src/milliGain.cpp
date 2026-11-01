#include "milliGain.h"

// Keeps whole * MILLI_PER_UNIT inside int16_t.
static const int16_t WHOLE_MAX = 32;
static const char DECIMAL_SEPARATOR = '.';
static const uint8_t FRACTION_DIGITS_MAX = 3;
static const uint8_t UNSIGNED_DIGITS_MAX = 6;

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

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

// Consumes ".d", ".dd" or ".ddd" and requires the string to end there; a
// missing fraction is 0. Short fractions are padded, so ".5" means 500.
static bool readFractionPart(const char **cursor, int16_t *thousandths)
{
    const char *reader = *cursor;
    int16_t scale = MILLI_PER_UNIT / 10;
    int16_t value = 0;
    uint8_t digits = 0;

    if (*reader == '\0') { *thousandths = 0; return true; }
    if (*reader != DECIMAL_SEPARATOR) return false;

    reader++;
    while (isDigit(*reader) && digits < FRACTION_DIGITS_MAX)
    {
        value = (int16_t)(value + (*reader - '0') * scale);
        scale = (int16_t)(scale / 10);
        reader++;
        digits++;
    }
    if (digits == 0 || *reader != '\0') return false;   // "1." and "1.2345" rejected

    *cursor = reader;
    *thousandths = value;
    return true;
}

bool parseMilliGain(const char *text, int16_t *out)
{
    if (text == NULL || out == NULL) return false;

    const char *cursor = text;
    int16_t whole = 0;
    int16_t thousandths = 0;

    if (!readWholePart(&cursor, &whole)) return false;
    if (!readFractionPart(&cursor, &thousandths)) return false;

    int32_t total = (int32_t)whole * MILLI_PER_UNIT + thousandths;
    if (total > INT16_MAX) return false;

    *out = (int16_t)total;
    return true;
}

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

static void writeFraction(char *out, uint16_t thousandths)
{
    out[0] = (char)('0' + thousandths / 100);
    out[1] = (char)('0' + (thousandths / 10) % 10);
    out[2] = (char)('0' + thousandths % 10);
}

void formatMilliGain(int16_t value, char *out, uint8_t len)
{
    if (out == NULL || len == 0) return;
    if (len < MILLI_TEXT_MIN_LEN || value < 0)
    {
        out[0] = '\0';
        return;
    }

    uint8_t index = writeUnsigned(out, (uint16_t)(value / MILLI_PER_UNIT));
    out[index++] = DECIMAL_SEPARATOR;
    writeFraction(out + index, (uint16_t)(value % MILLI_PER_UNIT));
    out[index + 3] = '\0';
}