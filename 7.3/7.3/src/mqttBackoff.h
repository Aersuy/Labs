#ifndef MQTT_BACKOFF_H
#define MQTT_BACKOFF_H

#include <stdint.h>

// Politica de reconectare, scoasa din taskul de retea ca sa fie verificabila
// fara broker: o reluare imediata la fiecare esec ar insemna sute de incercari
// pe minut cand WiFi-ul lipseste, iar serverul demo limiteaza conexiunile.
constexpr uint32_t MQTT_RECONNECT_DELAY_MIN_MS = 1000;
constexpr uint32_t MQTT_RECONNECT_DELAY_MAX_MS = 30000;

// failedAttempts este numarul de esecuri consecutive de pana acum: 0 inseamna
// prima reluare. Intervalul se dubleaza si se opreste la maxim.
uint32_t reconnectDelayMs(uint8_t failedAttempts);

#endif
