#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#include "uartStdio.h"
#include "echoDistance.h"
#include "distancePayload.h"
#include "frameCodec.h"
#include "i2cMasterLink.h"
#include "i2cProtocol.h"

// Recurenta ceruta pentru raportare, aceeasi ca in labele anterioare. Tactul
// FreeRTOS pe AVR are 16 ms, deci perioada reala este 496 ms.
constexpr TickType_t QUERY_PERIOD_MS = 500;

constexpr uint16_t QUERY_STACK = 256;
constexpr uint16_t REPORT_STACK = 256;         // printf are nevoie de stiva

// Ce a aflat o interogare. Starile se verifica in ordinea in care apar aici:
// mai intai magistrala, apoi anvelopa cadrului, apoi intelesul PAYLOAD-ului.
// Cand o verificare pica, campurile de dupa ea nu au inteles.
struct MasterUpdate {
    QueryStatus queryStatus;
    FrameStatus frameStatus;
    uint8_t receivedLen;
    bool isPayloadValid;
    DistanceReport report;
};

static QueueHandle_t xUpdateMailbox = NULL;

// Singura functie a masterului care atinge magistrala; restul lucreaza pe
// rezultatul ei.
static MasterUpdate runQuery()
{
    MasterUpdate update = {};
    uint8_t frame[FRAME_MAX_LEN];

    update.queryStatus = queryDistanceFrame(frame, DISTANCE_FRAME_LEN, &update.receivedLen);
    if (update.queryStatus != QUERY_OK) return update;

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = 0;
    update.frameStatus = decodeFrame(frame, update.receivedLen, payload, &payloadLen);
    if (update.frameStatus != FRAME_OK) return update;

    update.isPayloadValid = unpackDistancePayload(payload, payloadLen, &update.report);
    return update;
}

// Deține legatura I2C. Interogheaza la perioada fixa si trece rezultatul mai
// departe, fara sa afiseze nimic: afisarea pe UART este sincrona si ar
// deforma recurenta.
static void queryTask(void *pvParameters)
{
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        MasterUpdate update = runQuery();
        xQueueOverwrite(xUpdateMailbox, &update);

        xTaskDelayUntil(&lastWake, pdMS_TO_TICKS(QUERY_PERIOD_MS));
    }
}

static void reportReading(uint8_t sensorIndex, const DistanceReading *reading)
{
    if (reading->status == READING_OK)
    {
        printf_P(PSTR("S%u=%u mm  "), sensorIndex + 1, reading->distanceMm);
        return;
    }
    printf_P(PSTR("S%u=%s  "), sensorIndex + 1, readingStatusName(reading->status));
}

static void reportReadings(const DistanceReport *report)
{
    printf_P(PSTR("[master] seq=%u  "), report->sequence);
    for (uint8_t i = 0; i < report->sensorCount; i++)
    {
        reportReading(i, &report->readings[i]);
    }
    printf_P(PSTR("\n"));
}

// Un cadru cu PAYLOAD gol este raspunsul sclavului la o comanda necunoscuta,
// deci se deosebeste de un cadru corupt.
static void reportInvalidPayload(const MasterUpdate *update)
{
    printf_P(PSTR("[master] PAYLOAD respins (%u octeti primiti)\n"),
             update->receivedLen);
}

static void reportUpdate(const MasterUpdate *update)
{
    if (update->queryStatus != QUERY_OK)
    {
        printf_P(PSTR("[master] interogare eșuata: %s\n"),
                 queryStatusName(update->queryStatus));
        return;
    }
    if (update->frameStatus != FRAME_OK)
    {
        printf_P(PSTR("[master] cadru invalid: %s\n"),
                 frameStatusName(update->frameStatus));
        return;
    }
    if (!update->isPayloadValid)
    {
        reportInvalidPayload(update);
        return;
    }
    reportReadings(&update->report);
}

// Deține afisarea. Pentru varianta cu LCD se schimba doar fluxul: Lcd din
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

static void printMasterWelcome()
{
    printf_P(PSTR("\n=== Lab 7.1: master I2C, interogare la %u ms ===\n"), QUERY_PERIOD_MS);
    printf_P(PSTR("Sclav      : adresa 0x%02X, comanda 0x%02X\n"),
             SLAVE_I2C_ADDRESS, CMD_READ_DISTANCES);
    printf_P(PSTR("Cadru      : HEAD LENGTH PAYLOAD CHECKSUM, %u octeti\n"),
             DISTANCE_FRAME_LEN);
    printf_P(PSTR("Magistrala : SDA 20, SCL 21 pe Mega (A4/A5 pe Uno), pull-up 4.7k\n"));
}

static bool prepareMailbox()
{
    xUpdateMailbox = xQueueCreate(1, sizeof(MasterUpdate));
    if (xUpdateMailbox != NULL) return true;

    printf_P(PSTR("[err] cutia postala nu s-a creat; masterul nu porneste\n"));
    return false;
}

// Se foloseste aceeasi varianta uartStdioInit() ca pe sclav, desi Mega are RAM
// de ajuns pentru rtosUartStdioInit(): asa cele doua programe au acelasi drum
// de ieșire si rolurile se pot inversa intre placi fara alte modificari.
//
// Interogarea are prioritatea mai mare: afisarea scrie sincron pe UART si nu
// are voie sa intarzie perioada magistralei.
void setup()
{
    uartStdioInit();
    printMasterWelcome();

    if (!prepareMailbox()) return;
    i2cMasterLinkBegin();

    xTaskCreate(reportTask, "REPORT", REPORT_STACK, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(queryTask, "QUERY", QUERY_STACK, NULL, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();
}

void loop() { }
