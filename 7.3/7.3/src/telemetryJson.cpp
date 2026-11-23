#include "telemetryJson.h"

#include <stdio.h>
#include <stddef.h>

#include "deciTemperature.h"

// Cheile sunt si numele seriilor de pe tabloul de bord ThingsBoard: widgetul de
// grafic cere "temperature", indicatorul cere "ledOn".
static const char TELEMETRY_FORMAT[] = "{\"temperature\":%s,\"ledOn\":%s}";

uint8_t formatTelemetry(int16_t deciCelsius, bool isLedOn, char *out, uint8_t len)
{
    if (out == NULL || len == 0) return 0;
    if (len < TELEMETRY_TEXT_MIN_LEN)
    {
        out[0] = '\0';
        return 0;
    }

    char temperatureText[DECI_TEXT_MIN_LEN];
    formatDeciCelsius(deciCelsius, temperatureText, sizeof(temperatureText));

    // Cel mai lung rezultat posibil are 37 de caractere, iar tamponul are cel
    // putin TELEMETRY_TEXT_MIN_LEN: snprintf nu poate trunchia aici.
    int written = snprintf(out, len, TELEMETRY_FORMAT, temperatureText,
                           isLedOn ? "true" : "false");
    return (uint8_t)written;
}
