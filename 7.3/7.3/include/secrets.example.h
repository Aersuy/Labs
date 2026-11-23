#ifndef SECRETS_H
#define SECRETS_H

#include <stdint.h>

// Model pentru include/secrets.h, care NU intra in git (vezi .gitignore):
// copiaza acest fisier ca secrets.h si completeaza valorile tale.
//
//   copy include\secrets.example.h include\secrets.h
//
// Simbolul de acces se ia din ThingsBoard: Entities -> Devices -> dispozitivul
// tau -> Copy access token. El este echivalentul unei parole, deci nu se pune
// in cod si nu se arata in capturi de ecran.

constexpr char WIFI_SSID[] = "numele-retelei";
constexpr char WIFI_PASSWORD[] = "parola-retelei";

// Serverul demo accepta MQTT simplu pe 1883, deci nu este nevoie de certificate
// pe placa. Pentru un server propriu se schimba doar aceste doua valori.
constexpr char TB_HOST[] = "demo.thingsboard.io";
constexpr uint16_t TB_PORT = 1883;

constexpr char TB_ACCESS_TOKEN[] = "simbolul-de-acces-al-dispozitivului";

#endif
