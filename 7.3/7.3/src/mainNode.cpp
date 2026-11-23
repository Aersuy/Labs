#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#include "uartStdio.h"
#include "ky013.h"
#include "ledClass.h"
#include "deciTemperature.h"
#include "binaryPacket.h"
#include "packetCodec.h"
#include "packetParser.h"
#include "packetPayload.h"
#include "packetFifo.h"
#include "sequenceTracker.h"
#include "responseBuilder.h"
#include "serialLink.h"

constexpr uint32_t LINK_BAUD = 9600;
constexpr uint8_t TEMPERATURE_PIN = A0;
// D8, nu D13: pe D13 clipeste vApplicationMallocFailedHook, iar un LED comandat
// prin protocol ar ascunde exact semnalul de RAM epuizat.
constexpr uint8_t PROTOCOL_LED_PIN = 8;

constexpr uint8_t REQUEST_FIFO_DEPTH = 4;
constexpr uint8_t RESPONSE_QUEUE_DEPTH = 2;

// Un pachet de 11 octeti la 9600 bauzi tine 11,5 ms, iar tamponul de receptie
// al lui SoftwareSerial are 64 de octeti: o trezire la fiecare tact (16 ms) nu
// poate pierde octeti.
constexpr TickType_t LINK_POLL_TICKS = 1;

constexpr uint16_t RECEIVE_STACK = 128;
constexpr uint16_t PROCESS_STACK = 192;
constexpr uint16_t RESPOND_STACK = 256;            // printf are nevoie de stiva

static ky013 temperatureSensor(TEMPERATURE_PIN);
static Led protocolLed(PROTOCOL_LED_PIN);

static PacketFifo requestFifo;
static QueueHandle_t xResponseQueue = NULL;
static SequenceTracker sequenceTracker;

// Contoare de diagnostic. Fiecare are un singur scriitor, iar un octet se
// citeste atomic pe AVR, deci cititorii din celelalte taskuri nu au nevoie de
// mutex; volatile exista pentru ca optimizatorul sa nu le tina in registre.
static volatile uint8_t corruptedPacketCount = 0;  // scris de linkReceiveTask
static volatile uint8_t foreignPacketCount = 0;    // scris de linkReceiveTask
static volatile uint8_t acceptedPacketCount = 0;   // scrise de packetProcessTask
static volatile uint8_t lostPacketCount = 0;
static volatile uint8_t duplicatePacketCount = 0;

static uint8_t saturatingIncrement(uint8_t value)
{
    return (value < 0xFF) ? (uint8_t)(value + 1) : value;
}

static uint8_t saturatingAdd(uint8_t value, uint8_t amount)
{
    uint16_t sum = (uint16_t)value + amount;
    return (sum > 0xFF) ? 0xFF : (uint8_t)sum;
}

// --- taskul de receptie: flux de octeti -> pachete in FIFO ------------------

// Un pachet valid care nu mai incape in FIFO primeste NACK imediat, din taskul
// de receptie: altfel masterul ar astepta pana la expirare fara sa afle de ce.
// Cand nici coada de raspunsuri nu are loc, raspunsul se pierde si expirarea
// masterului este singura plasa de siguranta - de aceea nu se asteapta aici,
// taskul de receptie nu are voie sa se blocheze.
static void refuseForFullFifo(const BinaryPacket *packet)
{
    SlaveResponse nack = buildFifoFullNack(packet, DEVICE_ID_SLAVE);
    xQueueSend(xResponseQueue, &nack.packet, 0);
}

static void acceptPacket(const BinaryPacket *packet)
{
    if (packet->deviceId != DEVICE_ID_SLAVE)       // filtru pe ID
    {
        foreignPacketCount = saturatingIncrement(foreignPacketCount);
        return;
    }
    if (!packetFifoPush(&requestFifo, packet)) refuseForFullFifo(packet);
}

static void consumeLinkByte(PacketParser *parser, uint8_t byte)
{
    BinaryPacket packet;
    ParseResult result = parserFeed(parser, byte, &packet);

    if (result == PARSE_DONE)
    {
        acceptPacket(&packet);
        return;
    }
    if (result != PARSE_NEED_MORE)
    {
        corruptedPacketCount = saturatingIncrement(corruptedPacketCount);
    }
}

// Detine interfata seriala dedicata la citire. Nu interpreteaza nimic: aduna
// octeti in analizor si pune pachetele intregi in FIFO.
static void linkReceiveTask(void *pvParameters)
{
    PacketParser parser;
    parserReset(&parser);

    for (;;)
    {
        uint8_t byte = 0;
        while (linkReadByte(&byte)) consumeLinkByte(&parser, byte);

        vTaskDelay(LINK_POLL_TICKS);
    }
}

// --- taskul de prelucrare: pachet -> decizie --------------------------------

static void trackSequence(uint8_t sequence)
{
    uint8_t lostCount = 0;
    SequenceStatus status = sequenceTrackerAccept(&sequenceTracker, sequence, &lostCount);

    if (status == SEQUENCE_GAP) lostPacketCount = saturatingAdd(lostPacketCount, lostCount);
    if (status == SEQUENCE_DUPLICATE)
    {
        duplicatePacketCount = saturatingIncrement(duplicatePacketCount);
    }
    acceptedPacketCount = saturatingIncrement(acceptedPacketCount);
}

// Senzorul nelegat citeste 0 pe ADC, ceea ce da o temperatura absurda (circa
// -273 C), nu o eroare: valoarea ajunge asa la master, care o vede.
static int16_t readTemperatureDeci()
{
    float scaled = temperatureSensor.readCelcius() * (float)DECI_PER_UNIT;
    return (int16_t)((scaled >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f));
}

static SlaveStats collectStats()
{
    SlaveStats stats;
    stats.acceptedCount = acceptedPacketCount;
    stats.fifoDroppedCount = packetFifoDroppedCount(&requestFifo);
    stats.lostCount = lostPacketCount;
    stats.corruptedCount = corruptedPacketCount;
    return stats;
}

// Partea impura a prelucrarii: citeste ADC-ul si contoarele si le trece ca
// argumente in buildResponse(), care ramane determinist.
static SlaveContext collectContext()
{
    SlaveContext context;
    context.deviceId = DEVICE_ID_SLAVE;
    context.temperatureDeci = readTemperatureDeci();
    context.stats = collectStats();
    return context;
}

static void applyLedAction(LedAction action)
{
    if (action == LED_ACTION_TURN_ON) protocolLed.turnOn();
    if (action == LED_ACTION_TURN_OFF) protocolLed.turnOff();
}

static void processRequest(const BinaryPacket *request)
{
    trackSequence(request->sequence);

    SlaveContext context = collectContext();
    SlaveResponse response = buildResponse(request, &context);

    applyLedAction(response.ledAction);
    xQueueSend(xResponseQueue, &response.packet, portMAX_DELAY);
}

// Detine FIFO-ul la citire. Blocheaza cat timp coada este goala, deci nu
// consuma procesor intre cereri.
static void packetProcessTask(void *pvParameters)
{
    BinaryPacket request;

    for (;;)
    {
        if (packetFifoPop(&requestFifo, &request, portMAX_DELAY)) processRequest(&request);
    }
}

// --- taskul de raspuns: decizie -> fir si jurnal ----------------------------

static void transmitResponse(const BinaryPacket *response)
{
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = encodePacket(response, frame);

    if (frameLen == 0)
    {
        printf_P(PSTR("[err] raspuns cu PAYLOAD prea lung (%u), nu s-a trimis\n"),
                 response->payloadLength);
        return;
    }
    linkWriteBytes(frame, frameLen);
}

static void traceResponse(const BinaryPacket *response)
{
    printf_P(PSTR("[sclav] seq=%u %s len=%u | ok=%u pierdute=%u dubluri=%u "
                  "corupte=%u plin=%u straine=%u\n"),
             response->sequence, commandName(response->command), response->payloadLength,
             acceptedPacketCount, lostPacketCount, duplicatePacketCount,
             corruptedPacketCount, packetFifoDroppedCount(&requestFifo),
             foreignPacketCount);
}

// Detine interfata seriala dedicata la scriere. Trimite intai si jurnalizeaza
// abia apoi: scrierea pe STDIO este sincrona si nu are voie sa intarzie
// raspunsul pe care masterul il asteapta cu expirare.
static void respondTask(void *pvParameters)
{
    BinaryPacket response;

    for (;;)
    {
        if (xQueueReceive(xResponseQueue, &response, portMAX_DELAY) == pdPASS)
        {
            transmitResponse(&response);
            traceResponse(&response);
        }
    }
}

// --- pornire ----------------------------------------------------------------

static void printNodeWelcome()
{
    printf_P(PSTR("\n=== Lab 7.3: nod de senzor si actuator, comandat prin MQTT ===\n"));
    printf_P(PSTR("Legatura : SoftwareSerial RX D10, TX D11 la %lu bauzi\n"), LINK_BAUD);
    printf_P(PSTR("Pachet   : STX ID SEQ CMD LEN PAYLOAD CHK ETX, maxim %u octeti\n"),
             PACKET_MAX_LEN);
    printf_P(PSTR("ID       : 0x%02X, FIFO de %u pachete\n"),
             DEVICE_ID_SLAVE, REQUEST_FIFO_DEPTH);
    printf_P(PSTR("Punte    : ESP32 pe legatura seriala, server MQTT ThingsBoard\n"));
    printf_P(PSTR("Senzor   : KY-013 pe A%u, LED pe D%u\n"),
             TEMPERATURE_PIN - A0, PROTOCOL_LED_PIN);
}

static bool prepareQueues()
{
    if (!packetFifoCreate(&requestFifo, REQUEST_FIFO_DEPTH))
    {
        printf_P(PSTR("[err] FIFO-ul nu s-a creat; sclavul nu porneste\n"));
        return false;
    }

    xResponseQueue = xQueueCreate(RESPONSE_QUEUE_DEPTH, sizeof(BinaryPacket));
    if (xResponseQueue != NULL) return true;

    printf_P(PSTR("[err] coada de raspunsuri nu s-a creat; sclavul nu porneste\n"));
    return false;
}

// Receptia are prioritatea cea mai mare: ea nu are voie sa piarda octeti.
// Jurnalul din taskul de raspuns scrie sincron pe STDIO, deci sta cel mai jos.
void setup()
{
    uartStdioInit();
    printNodeWelcome();

    if (!prepareQueues()) return;

    sequenceTrackerReset(&sequenceTracker);
    temperatureSensor.ky013Init();
    protocolLed.begin();
    linkBegin(LINK_BAUD);

    xTaskCreate(respondTask, "RESP", RESPOND_STACK, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(packetProcessTask, "PROC", PROCESS_STACK, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(linkReceiveTask, "RECV", RECEIVE_STACK, NULL, tskIDLE_PRIORITY + 3, NULL);

    vTaskStartScheduler();
}

void loop() { }
