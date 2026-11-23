#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "mqttSession.h"
#include "thingsBoardTopics.h"

// Telemetria si confirmarile RPC au cateva zeci de octeti, iar apelurile
// primite sunt oprite la 256 in rpcCommand: 512 lasa loc pentru un camp nou
// fara retusuri. Valoarea implicita a bibliotecii este 256.
constexpr uint16_t MQTT_BUFFER_SIZE = 512;

// Tabloul de bord trebuie sa observe repede o placa disparuta, iar expirarea
// socketului tine conectarea sa nu blocheze taskul de retea mai mult de atat.
constexpr uint16_t MQTT_KEEPALIVE_S = 30;
constexpr uint16_t MQTT_SOCKET_TIMEOUT_S = 5;

// Identificatorul clientului trebuie doar sa fie unic pe broker; ThingsBoard
// recunoaste dispozitivul dupa simbolul de acces, nu dupa el.
constexpr char MQTT_CLIENT_ID[] = "lab73-gateway";

static WiFiClient networkClient;
static PubSubClient mqttClient(networkClient);
static MqttMessageHandler messageHandler = NULL;
static const char *deviceAccessToken = NULL;

// PubSubClient cere o functie libera cu semnatura proprie; ea doar traduce
// tipurile si lungimea catre functia de tratare a aplicatiei.
static void onMqttMessage(char *topic, uint8_t *payload, unsigned int length)
{
    if (messageHandler == NULL) return;

    messageHandler(topic, (const char *)payload, (uint16_t)length);
}

void mqttSessionBegin(const char *host, uint16_t port, const char *accessToken,
                      MqttMessageHandler handler)
{
    deviceAccessToken = accessToken;
    messageHandler = handler;

    mqttClient.setServer(host, port);
    mqttClient.setCallback(onMqttMessage);
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE_S);
    mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
}

// Abonamentul se reface la fiecare conectare reusita: brokerul il uita odata cu
// conexiunea, iar un gateway conectat dar neabonat ar trimite telemetrie si ar
// parea sanatos, fara sa mai primeasca nici o comanda.
bool mqttSessionConnect()
{
    // ThingsBoard recunoaste dispozitivul numai prin simbolul de acces, trecut
    // ca nume de utilizator: parola este goala, nu lipsa.
    const char *emptyPassword = NULL;

    if (!mqttClient.connect(MQTT_CLIENT_ID, deviceAccessToken, emptyPassword)) return false;

    return mqttClient.subscribe(TB_RPC_REQUEST_FILTER);
}

bool isMqttConnected()
{
    return mqttClient.connected();
}

bool mqttSessionPublish(const char *topic, const char *payload)
{
    return mqttClient.publish(topic, payload);
}

void mqttSessionPoll()
{
    mqttClient.loop();
}

int mqttSessionLastState()
{
    return mqttClient.state();
}
