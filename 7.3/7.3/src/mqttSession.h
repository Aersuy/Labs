#ifndef MQTT_SESSION_H
#define MQTT_SESSION_H

#include <stdint.h>

// Sesiunea MQTT: singurul loc care atinge clientul de retea. PubSubClient nu
// este reentrant, deci tot ce se vede aici se apeleaza dintr-un singur task -
// invariantul este scris aici pentru ca el nu poate fi impus de compilator.
//
// payload nu este terminat cu zero: lungimea vine separat, exact cum o da
// brokerul. Functia de tratare se apeleaza din mqttSessionPoll(), deci in
// acelasi task.
typedef void (*MqttMessageHandler)(const char *topic, const char *payload,
                                   uint16_t length);

// accessToken este simbolul de acces al dispozitivului ThingsBoard, folosit ca
// nume de utilizator MQTT; parola ramane goala.
void mqttSessionBegin(const char *host, uint16_t port, const char *accessToken,
                      MqttMessageHandler handler);

// O singura incercare de conectare, fara asteptare interioara: cat se asteapta
// intre incercari decide apelantul, cu reconnectDelayMs(). La reusita se face
// si abonarea la apelurile RPC, pentru ca un abonament se pierde odata cu
// conexiunea.
bool mqttSessionConnect();

bool isMqttConnected();

// payload trebuie sa fie un sir terminat cu zero.
bool mqttSessionPublish(const char *topic, const char *payload);

// Intretine conexiunea si livreaza mesajele primite catre functia de tratare.
void mqttSessionPoll();

// Starea raportata de PubSubClient dupa un esec (codurile MQTT_*), pentru
// jurnal: deosebeste "nu am ajuns la server" de "simbol de acces refuzat".
int mqttSessionLastState();

#endif
