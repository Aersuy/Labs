#ifndef WIFI_LINK_H
#define WIFI_LINK_H

#include <stdint.h>

// Conexiunea WiFi a punctii. Datele de autentificare se primesc ca argumente:
// ele sunt citite din secrets.h in mainGateway.cpp, la marginea aplicatiei, si
// nu sunt cunoscute de restul modulelor.
void wifiBegin(const char *ssid, const char *password);

bool isWifiConnected();

// De cate ori a primit placa o adresa IP de la pornire, pentru jurnal: prima
// conectare da 1, iar un numar care creste inseamna un WiFi care cade - asta
// explica golurile din grafic.
uint16_t wifiConnectCount();

#endif
