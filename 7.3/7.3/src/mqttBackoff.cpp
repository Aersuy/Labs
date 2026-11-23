#include "mqttBackoff.h"

// Dublarea se face prin inmultire intr-o bucla marginita, nu prin deplasare pe
// numarul de esecuri: asa saturarea la MQTT_RECONNECT_DELAY_MAX_MS se vede in
// cod si nu exista cale de depasire pentru un contor mare.
uint32_t reconnectDelayMs(uint8_t failedAttempts)
{
    uint32_t delay = MQTT_RECONNECT_DELAY_MIN_MS;

    for (uint8_t step = 0; step < failedAttempts; step++)
    {
        if (delay >= MQTT_RECONNECT_DELAY_MAX_MS / 2) return MQTT_RECONNECT_DELAY_MAX_MS;
        delay *= 2;
    }
    return delay;
}
