#ifndef THINGS_BOARD_TOPICS_H
#define THINGS_BOARD_TOPICS_H

#include <stdint.h>

// Contractul cu serverul MQTT, singura sursa pentru punte si pentru modulele
// pure: un subiect scris de doua ori, cu o litera in plus intr-un loc, nu ar
// da eroare de compilare - mesajele ar pleca pur si simplu in gol.
//
// Numele sunt impuse de ThingsBoard, nu alese de noi: telemetria merge pe un
// subiect fix, iar apelurile de la server vin pe .../rpc/request/<id> si se
// confirma pe .../rpc/response/<acelasi id>.
constexpr char TB_TELEMETRY_TOPIC[] = "v1/devices/me/telemetry";
constexpr char TB_ATTRIBUTES_TOPIC[] = "v1/devices/me/attributes";

constexpr char TB_RPC_REQUEST_FILTER[] = "v1/devices/me/rpc/request/+";
constexpr char TB_RPC_REQUEST_PREFIX[] = "v1/devices/me/rpc/request/";
constexpr char TB_RPC_RESPONSE_PREFIX[] = "v1/devices/me/rpc/response/";

// Metodele pe care le accepta puntea. Widgetul de comutator din tabloul de bord
// trimite setLedState; getLedState exista ca sa poata fi citita starea reala a
// actuatorului fara sa fie schimbata.
constexpr char RPC_METHOD_SET_LED[] = "setLedState";
constexpr char RPC_METHOD_GET_LED[] = "getLedState";

#endif
