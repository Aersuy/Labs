#ifndef TELEMETRY_JSON_H
#define TELEMETRY_JSON_H

#include <stdint.h>

// Corpul mesajului de telemetrie pentru ThingsBoard, construit determinist din
// valorile deja citite: {"temperature":23.4,"ledOn":true}. Numele cheilor sunt
// si numele seriilor din tabloul de bord, deci se schimba in acelasi loc.
//
// Forma este fixa, deci se scrie cu snprintf si cu formatDeciCelsius() care
// exista deja in proiect - nu are rost o biblioteca de JSON pentru asta.
// Analiza cererilor venite de la server este alta poveste: acolo forma nu este
// sub controlul nostru, vezi rpcCommand.h.

// Cel mai mic tampon acceptat: cheile, "-3276.8", "false" si terminatorul.
constexpr uint8_t TELEMETRY_TEXT_MIN_LEN = 48;

// Intoarce numarul de caractere scrise (fara terminator), sau 0 daca tamponul
// este prea mic - caz in care out primeste un sir vid, nu JSON trunchiat.
uint8_t formatTelemetry(int16_t deciCelsius, bool isLedOn, char *out, uint8_t len);

#endif
