#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#include "uartStdio.h"
#include "deciTemperature.h"
#include "binaryPacket.h"
#include "packetCodec.h"
#include "packetParser.h"
#include "packetPayload.h"
#include "packetFifo.h"
#include "requestBuilder.h"
#include "serialLink.h"

constexpr uint32_t LINK_BAUD = 9600;

// Recurenta ceruta pentru raportare, aceeasi ca in labele anterioare. Tactul
// FreeRTOS pe AVR are 16 ms, deci perioada reala este 496 ms.
constexpr TickType_t REQUEST_PERIOD_MS = 500;

// Sclavul are nevoie de cel mult un tact ca sa observe octetii (16 ms), de
// circa 12 ms ca sa emita raspunsul si de inca un tact ca sa fie observat aici.
constexpr TickType_t RESPONSE_TIMEOUT_MS = 150;
constexpr uint8_t MAX_ATTEMPTS = 3;                // o cerere si doua reluari

constexpr uint8_t RESPONSE_FIFO_DEPTH = 2;

constexpr uint16_t RECEIVE_STACK = 128;
constexpr uint16_t REQUEST_STACK = 192;
constexpr uint16_t REPORT_STACK = 256;             // printf are nevoie de stiva

// Cererile se rotesc, ca o singura rulare sa arate tot protocolul: liveness,
// citire de senzor, comanda cu PAYLOAD si diagnostic.
static const uint8_t COMMAND_SCHEDULE[] = {
    CMD_PING, CMD_READ_TEMP, CMD_SET_LED, CMD_GET_STATS
};
constexpr uint8_t COMMAND_SCHEDULE_LEN = sizeof(COMMAND_SCHEDULE);

enum ExchangeStatus : uint8_t {
    EXCHANGE_OK,
    EXCHANGE_TIMEOUT
};

// Rezultatul unui schimb, in ordinea in care se verifica: daca a venit un
// raspuns, apoi ce inseamna el. Cand status este EXCHANGE_TIMEOUT, response nu
// are inteles.
struct MasterUpdate {
    ExchangeStatus status;
    uint8_t attempts;
    BinaryPacket request;
    BinaryPacket response;
};

static PacketFifo responseFifo;
static QueueHandle_t xUpdateMailbox = NULL;

// Scris de linkReceiveTask, citit de reportTask: un octet se citeste atomic pe
// AVR, deci nu este nevoie de mutex.
static volatile uint8_t corruptedPacketCount = 0;

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
    if (result != PARSE_NEED_MORE && corruptedPacketCount < 0xFF)
    {
        corruptedPacketCount++;
    }
}

// Detine interfata seriala dedicata la citire. Separata de taskul de cereri ca
// expirarea sa fie o simpla asteptare pe FIFO, nu o bucla de sondare a portului.
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

// --- taskul de cereri: cerere, asteptare, retransmisie ----------------------

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

// Reluarea pastreaza acelasi SEQ: sclavul recunoaste retransmisia ca dublura,
// nu ca pachet nou, deci contorul lui de pierderi ramane corect.
static MasterUpdate runExchange(uint8_t command, uint8_t sequence, bool isLedOn)
{
    MasterUpdate update;
    update.request = buildRequest(command, sequence, isLedOn);

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

static uint8_t nextScheduleIndex(uint8_t scheduleIndex)
{
    return (uint8_t)((scheduleIndex + 1) % COMMAND_SCHEDULE_LEN);
}

// Detine legatura seriala la scriere si numarul de ordine. Nu afiseaza nimic:
// afisarea pe STDIO este sincrona si ar deforma recurenta.
static void requestTask(void *pvParameters)
{
    TickType_t lastWake = xTaskGetTickCount();
    uint8_t sequence = 0;
    uint8_t scheduleIndex = 0;
    bool isLedOn = false;

    for (;;)
    {
        uint8_t command = COMMAND_SCHEDULE[scheduleIndex];
        if (command == CMD_SET_LED) isLedOn = !isLedOn;

        MasterUpdate update = runExchange(command, sequence, isLedOn);
        xQueueOverwrite(xUpdateMailbox, &update);

        sequence++;
        scheduleIndex = nextScheduleIndex(scheduleIndex);
        xTaskDelayUntil(&lastWake, pdMS_TO_TICKS(REQUEST_PERIOD_MS));
    }
}

// --- taskul de raportare: raspuns -> STDIO ----------------------------------

static void reportTemperature(const BinaryPacket *response)
{
    int16_t deciCelsius = 0;
    if (!unpackTemperature(response->payload, response->payloadLength, &deciCelsius))
    {
        printf_P(PSTR("PAYLOAD de temperatura invalid (%u octeti)\n"),
                 response->payloadLength);
        return;
    }

    char text[DECI_TEXT_MIN_LEN];
    formatDeciCelsius(deciCelsius, text, sizeof(text));
    printf_P(PSTR("temperatura %s C\n"), text);
}

static void reportStats(const BinaryPacket *response)
{
    SlaveStats stats;
    if (!unpackStats(response->payload, response->payloadLength, &stats))
    {
        printf_P(PSTR("PAYLOAD de diagnostic invalid (%u octeti)\n"),
                 response->payloadLength);
        return;
    }

    printf_P(PSTR("sclav: ok=%u plin=%u pierdute=%u corupte=%u\n"),
             stats.acceptedCount, stats.fifoDroppedCount,
             stats.lostCount, stats.corruptedCount);
}

static void reportData(const BinaryPacket *response, uint8_t requestedCommand)
{
    if (requestedCommand == CMD_READ_TEMP)
    {
        reportTemperature(response);
        return;
    }
    if (requestedCommand == CMD_GET_STATS)
    {
        reportStats(response);
        return;
    }
    printf_P(PSTR("DATA neasteptat pentru %s\n"), commandName(requestedCommand));
}

static void reportNack(const BinaryPacket *response)
{
    if (response->payloadLength != NACK_PAYLOAD_LEN)
    {
        printf_P(PSTR("NACK fara motiv\n"));
        return;
    }
    printf_P(PSTR("NACK, motiv %u\n"), response->payload[0]);
}

static void reportAck(const BinaryPacket *response)
{
    if (response->payloadLength == LED_PAYLOAD_LEN)
    {
        printf_P(PSTR("ACK, LED %s\n"), response->payload[0] ? "aprins" : "stins");
        return;
    }
    printf_P(PSTR("ACK\n"));
}

static void reportResponse(const MasterUpdate *update)
{
    const BinaryPacket *response = &update->response;

    switch (response->command)
    {
        case RESP_ACK:  reportAck(response); return;
        case RESP_DATA: reportData(response, update->request.command); return;
        case RESP_NACK: reportNack(response); return;
        default: break;
    }
    printf_P(PSTR("raspuns necunoscut 0x%02X\n"), response->command);
}

static void reportUpdate(const MasterUpdate *update)
{
    printf_P(PSTR("[master] seq=%u %s inc=%u: "), update->request.sequence,
             commandName(update->request.command), update->attempts);

    if (update->status == EXCHANGE_TIMEOUT)
    {
        printf_P(PSTR("fara raspuns (corupte=%u plin=%u)\n"),
                 corruptedPacketCount, packetFifoDroppedCount(&responseFifo));
        return;
    }
    reportResponse(update);
}

// Detine afisarea. Pentru varianta cu LCD se schimba doar fluxul: Lcd din
// ../../drivers redirecteaza un FILE*, deci functiile de raportare raman
// neschimbate.
static void reportTask(void *pvParameters)
{
    MasterUpdate update;

    for (;;)
    {
        if (xQueueReceive(xUpdateMailbox, &update, portMAX_DELAY) == pdPASS)
        {
            reportUpdate(&update);
        }
    }
}

// --- pornire ----------------------------------------------------------------

static void printMasterWelcome()
{
    printf_P(PSTR("\n=== Lab 7.2: master, protocol logic binar la %u ms ===\n"),
             (unsigned)REQUEST_PERIOD_MS);
    printf_P(PSTR("Legatura : SoftwareSerial RX D10, TX D11 la %lu bauzi\n"), LINK_BAUD);
    printf_P(PSTR("Pachet   : STX ID SEQ CMD LEN PAYLOAD CHK ETX, maxim %u octeti\n"),
             PACKET_MAX_LEN);
    printf_P(PSTR("Sclav    : ID 0x%02X, expirare %u ms, %u incercari\n"),
             DEVICE_ID_SLAVE, (unsigned)RESPONSE_TIMEOUT_MS, MAX_ATTEMPTS);
}

static bool prepareQueues()
{
    if (!packetFifoCreate(&responseFifo, RESPONSE_FIFO_DEPTH))
    {
        printf_P(PSTR("[err] FIFO-ul nu s-a creat; masterul nu porneste\n"));
        return false;
    }

    xUpdateMailbox = xQueueCreate(1, sizeof(MasterUpdate));
    if (xUpdateMailbox != NULL) return true;

    printf_P(PSTR("[err] cutia postala nu s-a creat; masterul nu porneste\n"));
    return false;
}

// Se foloseste uartStdioInit() (nu varianta cu task si cozi proprii) pentru ca
// pe ATmega328P nu incape: taskul UART si cele doua cozi ale lui cer ~280 de
// octeti, iar SoftwareSerial isi ia deja tamponul propriu de receptie. Daca
// RAM-ul se termina, LED-ul de pe D13 clipeste rapid
// (vApplicationMallocFailedHook).
//
// Receptia are prioritatea cea mai mare, raportarea cea mai mica: afisarea
// scrie sincron pe STDIO si nu are voie sa intarzie recurenta cererilor.
void setup()
{
    uartStdioInit();
    printMasterWelcome();

    if (!prepareQueues()) return;
    linkBegin(LINK_BAUD);

    xTaskCreate(reportTask, "REPORT", REPORT_STACK, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(requestTask, "REQ", REQUEST_STACK, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(linkReceiveTask, "RECV", RECEIVE_STACK, NULL, tskIDLE_PRIORITY + 3, NULL);

    vTaskStartScheduler();
}

void loop() { }
