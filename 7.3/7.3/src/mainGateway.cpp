#include <Arduino.h>

#include "rtosPort.h"
#include "secrets.h"
#include "binaryPacket.h"
#include "packetCodec.h"
#include "packetParser.h"
#include "packetPayload.h"
#include "packetFifo.h"
#include "requestBuilder.h"
#include "serialLink.h"
#include "wifiLink.h"
#include "mqttSession.h"
#include "thingsBoardTopics.h"
#include "telemetryJson.h"
#include "rpcCommand.h"
#include "rpcResponse.h"
#include "mqttBackoff.h"

// Puntea dintre protocolul binar din 7.2 si resursa din Internet: pe fir este
// masterul de atunci (intreaba nodul, cu expirare si retransmisie), iar spre
// ThingsBoard este un client MQTT care publica telemetrie si primeste apeluri
// RPC pentru actuator.
//
//   nod (Uno) --Serial2--> linkReceiveTask --> responseFifo
//                          exchangeTask   --> xPublishQueue --> mqttTask --> MQTT
//                          xCommandQueue  <-------------------- mqttTask <-- RPC
//
// Comanda primita de la tabloul de bord nu se executa in taskul de retea: ea
// trece la taskul care detine legatura seriala, iar confirmarea pleaca abia
// dupa ce nodul a raspuns. Asa comutatorul din tablou arata ce face
// actuatorul, nu ce i s-a cerut.

constexpr uint32_t LINK_BAUD = 9600;
constexpr uint32_t LOG_BAUD = 115200;

// Lasa portul USB sa se asaze inainte de primul mesaj, altfel bannerul se
// pierde cand monitorul este deschis chiar la pornire.
constexpr uint32_t LOG_SETTLE_MS = 300;

// Recurenta telemetriei. Tactul FreeRTOS pe ESP32 este de 1 ms, deci perioada
// chiar este 1000 ms (pe AVR, cu tact de 16 ms, ar fi fost 992). Serverul demo
// limiteaza mesajele pe dispozitiv, deci citirea si publicarea merg la secunda,
// nu la 500 ms ca in laboratoarele dinainte.
constexpr TickType_t TELEMETRY_PERIOD_MS = 1000;

// Cat de des trece mqttTask: suficient de des ca un apel RPC sa para
// instantaneu, suficient de rar ca taskul sa nu ocupe procesorul degeaba.
constexpr TickType_t MQTT_POLL_MS = 10;
constexpr TickType_t WIFI_WAIT_MS = 500;

// Aceleasi valori ca in 7.2: nodul are nevoie de cel mult un tact ca sa observe
// octetii, de circa 12 ms ca sa emita raspunsul si de inca unul pana aici.
constexpr TickType_t RESPONSE_TIMEOUT_MS = 150;
constexpr uint8_t MAX_ATTEMPTS = 3;                // o cerere si doua reluari

constexpr uint8_t RESPONSE_FIFO_DEPTH = 2;
constexpr uint8_t COMMAND_QUEUE_DEPTH = 4;
constexpr uint8_t PUBLISH_QUEUE_DEPTH = 4;

// Cat accepta sa astepte exchangeTask ca un rezultat sa intre in coada de
// publicare: cat timp reteaua lipseste, mqttTask nu goleste coada si rezultatele
// vechi nu mai au valoare.
constexpr TickType_t PUBLISH_SEND_TIMEOUT_MS = 50;

// Pe ESP32 stivele se dau in octeti, nu in cuvinte ca pe AVR.
constexpr uint32_t RECEIVE_STACK = 2048;
constexpr uint32_t EXCHANGE_STACK = 3072;
constexpr uint32_t MQTT_STACK = 4096;

// Taskul Arduino (setup/loop) ruleaza pe prioritatea 1, deci toate trei sunt
// peste el. Receptia este cea mai de sus: ea nu are voie sa piarda octeti.
constexpr UBaseType_t MQTT_PRIORITY = 2;
constexpr UBaseType_t EXCHANGE_PRIORITY = 3;
constexpr UBaseType_t RECEIVE_PRIORITY = 4;

enum ExchangeStatus : uint8_t {
    EXCHANGE_OK,
    EXCHANGE_TIMEOUT
};

// Ce a cerut tabloul de bord. requestId este numarul apelului RPC, pe care
// confirmarea trebuie sa il poarte inapoi neschimbat.
struct LedCommand {
    uint32_t requestId;
    bool isOn;
};

// Rezultatul unui schimb pe fir, in ordinea in care se verifica: daca a venit
// un raspuns, apoi ce inseamna el. Cand status este EXCHANGE_TIMEOUT, response
// nu are inteles.
struct GatewayUpdate {
    ExchangeStatus status;
    uint8_t attempts;
    BinaryPacket request;
    BinaryPacket response;
    uint32_t rpcRequestId;
    bool needsRpcReply;
};

static PacketFifo responseFifo;
static QueueHandle_t xCommandQueue = NULL;
static QueueHandle_t xPublishQueue = NULL;

// Contoare de diagnostic, fiecare cu un singur scriitor; cititorul este
// jurnalul din mqttTask. volatile exista ca optimizatorul sa nu le tina in
// registre.
static volatile uint8_t corruptedPacketCount = 0;   // scris de linkReceiveTask
static volatile uint8_t publishDroppedCount = 0;    // scris de exchangeTask

static uint8_t saturatingIncrement(uint8_t value)
{
    return (value < 0xFF) ? (uint8_t)(value + 1) : value;
}

// --- taskul de receptie: flux de octeti -> pachete in FIFO ------------------

static void consumeLinkByte(PacketParser *parser, uint8_t byte)
{
    BinaryPacket packet;
    ParseResult result = parserFeed(parser, byte, &packet);

    if (result == PARSE_DONE)
    {
        packetFifoPush(&responseFifo, &packet);
        return;
    }
    if (result != PARSE_NEED_MORE)
    {
        corruptedPacketCount = saturatingIncrement(corruptedPacketCount);
    }
}

// Detine interfata seriala dedicata la citire. Separata de taskul de schimburi
// ca expirarea sa fie o simpla asteptare pe FIFO, nu o bucla de sondare.
static void linkReceiveTask(void *pvParameters)
{
    PacketParser parser;
    parserReset(&parser);

    for (;;)
    {
        uint8_t byte = 0;
        while (linkReadByte(&byte)) consumeLinkByte(&parser, byte);

        vTaskDelay(1);
    }
}

// --- taskul de schimburi: cerere, asteptare, retransmisie -------------------

static void transmitRequest(const BinaryPacket *request)
{
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = encodePacket(request, frame);

    linkWriteBytes(frame, frameLen);
}

// Pachetele care nu raspund cererii curente (ecoul intarziat al unei
// retransmisii anterioare) se arunca, dar nu reinnoiesc rabdarea: bugetul de
// timp scade cu cat s-a asteptat deja.
static bool awaitResponse(const BinaryPacket *request, BinaryPacket *responseOut)
{
    TickType_t remaining = pdMS_TO_TICKS(RESPONSE_TIMEOUT_MS);

    while (remaining > 0)
    {
        TickType_t startTick = xTaskGetTickCount();
        if (!packetFifoPop(&responseFifo, responseOut, remaining)) return false;
        if (isResponseToRequest(responseOut, request)) return true;

        TickType_t spent = (TickType_t)(xTaskGetTickCount() - startTick);
        remaining = (spent >= remaining) ? 0 : (TickType_t)(remaining - spent);
    }
    return false;
}

// Reluarea pastreaza acelasi SEQ: nodul recunoaste retransmisia ca dublura, nu
// ca pachet nou, deci contorul lui de pierderi ramane corect.
static GatewayUpdate runExchange(uint8_t command, uint8_t sequence, bool isLedOn)
{
    GatewayUpdate update;
    update.request = buildRequest(command, sequence, isLedOn);
    update.rpcRequestId = 0;
    update.needsRpcReply = false;

    for (uint8_t attempt = 1; attempt <= MAX_ATTEMPTS; attempt++)
    {
        update.attempts = attempt;
        transmitRequest(&update.request);

        if (awaitResponse(&update.request, &update.response))
        {
            update.status = EXCHANGE_OK;
            return update;
        }
    }
    update.status = EXCHANGE_TIMEOUT;
    return update;
}

static void queueForPublishing(const GatewayUpdate *update)
{
    if (xQueueSend(xPublishQueue, update, pdMS_TO_TICKS(PUBLISH_SEND_TIMEOUT_MS)) == pdPASS)
    {
        return;
    }
    publishDroppedCount = saturatingIncrement(publishDroppedCount);
}

static void runLedExchange(const LedCommand *command, uint8_t sequence)
{
    GatewayUpdate update = runExchange(CMD_SET_LED, sequence, command->isOn);
    update.rpcRequestId = command->requestId;
    update.needsRpcReply = true;

    queueForPublishing(&update);
}

static void runTemperatureExchange(uint8_t sequence)
{
    GatewayUpdate update = runExchange(CMD_READ_TEMP, sequence, false);

    queueForPublishing(&update);
}

// Diferenta se calculeaza pe intreg cu semn ca trecerea contorului de tacturi
// peste maxim sa nu arate ca un termen foarte indepartat.
static TickType_t ticksUntil(TickType_t deadline)
{
    TickType_t now = xTaskGetTickCount();

    if ((int32_t)(deadline - now) <= 0) return 0;
    return (TickType_t)(deadline - now);
}

// Daca o comanda de LED a mancat intervalul, citirile nu se inghesuie una dupa
// alta ca sa recupereze: termenul sare inainte, fara restante.
static TickType_t nextDeadline(TickType_t deadline, TickType_t period)
{
    TickType_t advanced = (TickType_t)(deadline + period);
    TickType_t now = xTaskGetTickCount();

    if ((int32_t)(advanced - now) <= 0) return (TickType_t)(now + period);
    return advanced;
}

// Detine legatura seriala la scriere si numarul de ordine. Asteptarea pe coada
// de comenzi este si ceasul citirilor: o comanda de la tabloul de bord intra
// imediat, iar daca nu vine nimic pana la termen se citeste temperatura.
static void exchangeTask(void *pvParameters)
{
    const TickType_t period = pdMS_TO_TICKS(TELEMETRY_PERIOD_MS);
    TickType_t nextTelemetry = (TickType_t)(xTaskGetTickCount() + period);
    uint8_t sequence = 0;

    for (;;)
    {
        LedCommand command;
        if (xQueueReceive(xCommandQueue, &command, ticksUntil(nextTelemetry)) == pdPASS)
        {
            runLedExchange(&command, sequence++);
            continue;
        }
        runTemperatureExchange(sequence++);
        nextTelemetry = nextDeadline(nextTelemetry, period);
    }
}

// --- taskul de retea: rezultate -> MQTT, apeluri RPC -> comenzi -------------

// Starea trimisa in Internet. Scrisa si citita numai din mqttTask, deci are un
// singur proprietar: nici mutex, nici volatile.
struct PublishedState {
    int16_t temperatureDeci;
    bool hasTemperature;
    bool isLedOn;
};

static PublishedState publishedState = { 0, false, false };

static void publishTelemetry()
{
    if (!publishedState.hasTemperature) return;

    char payload[TELEMETRY_TEXT_MIN_LEN];
    formatTelemetry(publishedState.temperatureDeci, publishedState.isLedOn,
                    payload, sizeof(payload));

    if (!mqttSessionPublish(TB_TELEMETRY_TOPIC, payload))
    {
        Serial.printf("[punte] telemetria nu a plecat (stare %d)\n", mqttSessionLastState());
        return;
    }
    Serial.printf("[punte] telemetrie %s\n", payload);
}

static void publishRpcReply(uint32_t requestId, bool isOn)
{
    char topic[RPC_RESPONSE_TOPIC_MIN_LEN];
    char payload[RPC_RESPONSE_TEXT_MIN_LEN];

    formatRpcResponseTopic(requestId, topic, sizeof(topic));
    formatRpcResponse(isOn, payload, sizeof(payload));

    if (mqttSessionPublish(topic, payload)) return;

    Serial.printf("[punte] confirmarea apelului %lu nu a plecat (stare %d)\n",
                  (unsigned long)requestId, mqttSessionLastState());
}

static bool applyTemperature(const BinaryPacket *response)
{
    if (response->command != RESP_DATA) return false;

    int16_t deciCelsius = 0;
    if (!unpackTemperature(response->payload, response->payloadLength, &deciCelsius))
    {
        return false;
    }

    publishedState.temperatureDeci = deciCelsius;
    publishedState.hasTemperature = true;
    return true;
}

// Starea LED-ului se ia din ecoul nodului, nu din ce i s-a cerut: daca nodul a
// refuzat comanda cu NACK, tabloul de bord trebuie sa vada starea veche.
static bool applyLedState(const BinaryPacket *response)
{
    if (response->command != RESP_ACK) return false;

    bool isOn = false;
    if (!unpackLedRequest(response->payload, response->payloadLength, &isOn)) return false;

    publishedState.isLedOn = isOn;
    return true;
}

static bool applyResponse(const GatewayUpdate *update)
{
    if (update->request.command == CMD_READ_TEMP) return applyTemperature(&update->response);
    if (update->request.command == CMD_SET_LED) return applyLedState(&update->response);
    return false;
}

static void logRejectedUpdate(const GatewayUpdate *update)
{
    if (update->status == EXCHANGE_TIMEOUT)
    {
        Serial.printf("[punte] %s seq=%u fara raspuns de la nod "
                      "(inc=%u corupte=%u nepublicate=%u)\n",
                      commandName(update->request.command), update->request.sequence,
                      update->attempts, corruptedPacketCount, publishDroppedCount);
        return;
    }
    Serial.printf("[punte] %s seq=%u: raspuns neasteptat %s (len=%u)\n",
                  commandName(update->request.command), update->request.sequence,
                  commandName(update->response.command), update->response.payloadLength);
}

// Confirmarea RPC pleaca si cand schimbul a esuat, cu starea cunoscuta: altfel
// comutatorul din tabloul de bord ar ramane in asteptare pana la expirarea lui.
static void handleUpdate(const GatewayUpdate *update)
{
    bool wasApplied = (update->status == EXCHANGE_OK) && applyResponse(update);

    if (!wasApplied) logRejectedUpdate(update);
    if (update->needsRpcReply) publishRpcReply(update->rpcRequestId, publishedState.isLedOn);
    if (wasApplied) publishTelemetry();
}

static void drainPublishQueue()
{
    GatewayUpdate update;

    while (xQueueReceive(xPublishQueue, &update, 0) == pdPASS) handleUpdate(&update);
}

static void enqueueLedCommand(const RpcRequest *request)
{
    LedCommand command;
    command.requestId = request->requestId;
    command.isOn = request->isOn;

    if (xQueueSend(xCommandQueue, &command, 0) == pdPASS) return;

    Serial.printf("[punte] coada de comenzi plina, apelul %lu s-a pierdut\n",
                  (unsigned long)request->requestId);
}

// Se apeleaza din mqttSessionPoll(), deci chiar in mqttTask: de aici se poate
// publica. Citirea starii se raspunde pe loc; comutarea pleaca spre taskul care
// detine legatura seriala.
//
// Ordinea nu este o preferinta: PubSubClient construieste mesajul de iesire in
// chiar tamponul in care stau topic si payload, deci nimic nu se publica
// inainte ca cererea sa fie citita pana la capat in parseRpcRequest().
static void handleIncomingMessage(const char *topic, const char *payload, uint16_t length)
{
    RpcRequest request;
    RpcParseResult result = parseRpcRequest(topic, payload, length, &request);

    if (result != RPC_PARSE_OK)
    {
        Serial.printf("[punte] apel RPC refuzat: %s\n", rpcParseResultName(result));
        return;
    }
    if (request.method == RPC_GET_LED)
    {
        publishRpcReply(request.requestId, publishedState.isLedOn);
        return;
    }
    enqueueLedCommand(&request);
}

// Intoarce numarul de esecuri consecutive de pana acum, de care depinde cat se
// asteapta pana la urmatoarea incercare.
static uint8_t connectOnce(uint8_t failedAttempts)
{
    if (mqttSessionConnect())
    {
        Serial.printf("[punte] conectat la %s:%u (abonat la %s)\n",
                      TB_HOST, TB_PORT, TB_RPC_REQUEST_FILTER);
        return 0;
    }

    uint32_t delayMs = reconnectDelayMs(failedAttempts);
    Serial.printf("[punte] conectare MQTT refuzata (stare %d), reiau in %lu ms\n",
                  mqttSessionLastState(), (unsigned long)delayMs);
    vTaskDelay(pdMS_TO_TICKS(delayMs));
    return saturatingIncrement(failedAttempts);
}

// Detine clientul MQTT si jurnalul. PubSubClient nu este reentrant, deci nici
// un alt task nu publica: celelalte trimit rezultate prin coada.
static void mqttTask(void *pvParameters)
{
    uint8_t failedAttempts = 0;

    for (;;)
    {
        if (!isWifiConnected())
        {
            vTaskDelay(pdMS_TO_TICKS(WIFI_WAIT_MS));
            continue;
        }
        if (!isMqttConnected())
        {
            failedAttempts = connectOnce(failedAttempts);
            continue;
        }
        mqttSessionPoll();
        drainPublishQueue();
        vTaskDelay(pdMS_TO_TICKS(MQTT_POLL_MS));
    }
}

// --- pornire ----------------------------------------------------------------

static void printGatewayWelcome()
{
    Serial.printf("\n=== Lab 7.3: punte MQTT, telemetrie la %lu ms ===\n",
                  (unsigned long)TELEMETRY_PERIOD_MS);
    Serial.printf("Legatura : Serial2 (RX2 GPIO16, TX2 GPIO17) la %lu bauzi\n",
                  (unsigned long)LINK_BAUD);
    Serial.printf("Pachet   : STX ID SEQ CMD LEN PAYLOAD CHK ETX, maxim %u octeti\n",
                  PACKET_MAX_LEN);
    Serial.printf("Nod      : ID 0x%02X, expirare %lu ms, %u incercari\n",
                  DEVICE_ID_SLAVE, (unsigned long)RESPONSE_TIMEOUT_MS, MAX_ATTEMPTS);
    Serial.printf("Server   : %s:%u, telemetrie pe %s\n", TB_HOST, TB_PORT, TB_TELEMETRY_TOPIC);
    Serial.printf("Comenzi  : %s / %s pe %s\n", RPC_METHOD_SET_LED, RPC_METHOD_GET_LED,
                  TB_RPC_REQUEST_FILTER);
}

static bool prepareQueues()
{
    if (!packetFifoCreate(&responseFifo, RESPONSE_FIFO_DEPTH))
    {
        Serial.printf("[err] FIFO-ul nu s-a creat; puntea nu porneste\n");
        return false;
    }

    xCommandQueue = xQueueCreate(COMMAND_QUEUE_DEPTH, sizeof(LedCommand));
    xPublishQueue = xQueueCreate(PUBLISH_QUEUE_DEPTH, sizeof(GatewayUpdate));
    if (xCommandQueue != NULL && xPublishQueue != NULL) return true;

    Serial.printf("[err] cozile nu s-au creat; puntea nu porneste\n");
    return false;
}

void setup()
{
    Serial.begin(LOG_BAUD);
    delay(LOG_SETTLE_MS);
    printGatewayWelcome();

    if (!prepareQueues()) return;

    linkBegin(LINK_BAUD);
    wifiBegin(WIFI_SSID, WIFI_PASSWORD);
    mqttSessionBegin(TB_HOST, TB_PORT, TB_ACCESS_TOKEN, handleIncomingMessage);

    xTaskCreate(mqttTask, "MQTT", MQTT_STACK, NULL, MQTT_PRIORITY, NULL);
    xTaskCreate(exchangeTask, "EXCH", EXCHANGE_STACK, NULL, EXCHANGE_PRIORITY, NULL);
    xTaskCreate(linkReceiveTask, "RECV", RECEIVE_STACK, NULL, RECEIVE_PRIORITY, NULL);
}

// Pe ESP32 planificatorul ruleaza deja cand se intra in setup(), deci nu se
// apeleaza vTaskStartScheduler() ca pe AVR. loop() este taskul Arduino, care
// nu mai are nimic de facut.
void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
