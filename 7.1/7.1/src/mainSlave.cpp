#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#include "uartStdio.h"
#include "hcsr04.h"
#include "echoDistance.h"
#include "distancePayload.h"
#include "frameCodec.h"
#include "distanceBuffer.h"
#include "i2cSlaveLink.h"
#include "i2cProtocol.h"

// Pinii 2..5 exista pe Uno si pe Mega, deci montajul senzorilor este acelasi
// pe ambele placi. A4/A5 (SDA/SCL pe Uno) raman libere pentru magistrala.
constexpr uint8_t SENSOR_ONE_TRIGGER_PIN = 2;
constexpr uint8_t SENSOR_ONE_ECHO_PIN = 3;
constexpr uint8_t SENSOR_TWO_TRIGGER_PIN = 4;
constexpr uint8_t SENSOR_TWO_ECHO_PIN = 5;

// Tactul FreeRTOS pe AVR vine din watchdog si are 16 ms, deci asteptarile se
// rotunjesc in jos la multipli de 16 ms: 80 ms -> 64 ms, 300 ms -> 288 ms.
// Pauza dintre senzori acopera cele ~60 ms cerute de fisa de catalog, ca ecoul
// primului senzor sa nu mai fie in aer cand emite al doilea.
constexpr TickType_t SENSOR_SETTLE_MS = 80;
constexpr TickType_t ACQUISITION_PERIOD_MS = 300;

constexpr uint16_t ACQUISITION_STACK = 192;
constexpr uint16_t PUBLISH_STACK = 256;        // printf are nevoie de stiva

static Hcsr04 sensors[SENSOR_COUNT] = {
    Hcsr04(SENSOR_ONE_TRIGGER_PIN, SENSOR_ONE_ECHO_PIN),
    Hcsr04(SENSOR_TWO_TRIGGER_PIN, SENSOR_TWO_ECHO_PIN)
};

static QueueHandle_t xReportMailbox = NULL;

static void beginSensors()
{
    for (uint8_t i = 0; i < SENSOR_COUNT; i++)
    {
        sensors[i].begin();
    }
}

// Senzorii se citesc pe rand: declansarea simultana ar insemna doua ecouri in
// aer in acelasi timp, iar fiecare receptor ar putea auzi burstul celuilalt.
static void measureAllSensors(DistanceReading *readingsOut)
{
    for (uint8_t i = 0; i < SENSOR_COUNT; i++)
    {
        readingsOut[i] = echoMicrosToReading(sensors[i].measureEchoMicros());
        vTaskDelay(pdMS_TO_TICKS(SENSOR_SETTLE_MS));
    }
}

// Deține senzorii. Nu cunoaste protocolul: masoara si trece rezultatul mai
// departe. Cutia poștala are un singur loc si se scrie cu xQueueOverwrite,
// deci o masuratoare noua inlocuieste una necitita - masterul vrea mereu
// ultima valoare, nu un istoric.
static void acquisitionTask(void *pvParameters)
{
    DistanceReport report = { 0, SENSOR_COUNT, {} };

    beginSensors();
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        measureAllSensors(report.readings);
        report.sequence++;
        xQueueOverwrite(xReportMailbox, &report);

        xTaskDelayUntil(&lastWake, pdMS_TO_TICKS(ACQUISITION_PERIOD_MS));
    }
}

// Formatarea cadrului se face aici, nu in ISR-ul TWI: ISR-ul trebuie sa fie
// doar o copiere.
static void publishReport(const DistanceReport *report)
{
    uint8_t payload[DISTANCE_PAYLOAD_LEN];
    uint8_t frame[FRAME_MAX_LEN];

    uint8_t payloadLen = packDistancePayload(report, payload);
    uint8_t frameLen = encodeFrame(payload, payloadLen, frame);
    publishFrame(frame, frameLen);
}

static void traceReading(uint8_t sensorIndex, const DistanceReading *reading)
{
    if (reading->status == READING_OK)
    {
        printf_P(PSTR("S%u=%u mm  "), sensorIndex + 1, reading->distanceMm);
        return;
    }
    printf_P(PSTR("S%u=%s  "), sensorIndex + 1, readingStatusName(reading->status));
}

static void traceReport(const DistanceReport *report)
{
    printf_P(PSTR("[sclav %u] "), report->sequence);
    for (uint8_t i = 0; i < report->sensorCount; i++)
    {
        traceReading(i, &report->readings[i]);
    }
    printf_P(PSTR("| comenzi necunoscute: %u\n"), i2cSlaveUnknownCommandCount());
}

// Deține tamponul de raspuns. Publica intai cadrul si abia apoi jurnalizeaza:
// scrierea pe UART este sincrona, deci nu are voie sa intarzie datele pe care
// masterul le poate cere in orice moment.
static void framePublishTask(void *pvParameters)
{
    DistanceReport report;

    for (;;)
    {
        if (xQueueReceive(xReportMailbox, &report, portMAX_DELAY) == pdPASS)
        {
            publishReport(&report);
            traceReport(&report);
        }
    }
}

static void printSlaveWelcome()
{
    printf_P(PSTR("\n=== Lab 7.1: sclav I2C cu doi senzori HC-SR04 (FreeRTOS) ===\n"));
    printf_P(PSTR("Adresa I2C : 0x%02X, comanda de citire: 0x%02X\n"),
             SLAVE_I2C_ADDRESS, CMD_READ_DISTANCES);
    printf_P(PSTR("Senzor 1   : TRIG D%u, ECHO D%u\n"),
             SENSOR_ONE_TRIGGER_PIN, SENSOR_ONE_ECHO_PIN);
    printf_P(PSTR("Senzor 2   : TRIG D%u, ECHO D%u\n"),
             SENSOR_TWO_TRIGGER_PIN, SENSOR_TWO_ECHO_PIN);
    printf_P(PSTR("Cadru      : HEAD LENGTH PAYLOAD CHECKSUM, %u octeti\n"),
             DISTANCE_FRAME_LEN);
}

// Un cadru valid trebuie sa existe inainte de deschiderea magistralei: o cerere
// sosita inaintea primei masuratori primeste stari "nemasurat", nu zero octeti.
static bool prepareSharedBuffer()
{
    xReportMailbox = xQueueCreate(1, sizeof(DistanceReport));
    if (xReportMailbox == NULL)
    {
        printf_P(PSTR("[err] cutia postala nu s-a creat; sclavul nu porneste\n"));
        return false;
    }

    DistanceReport emptyReport = { 0, SENSOR_COUNT, {} };
    publishReport(&emptyReport);
    return true;
}

// Varianta uartStdioInit() (nu cea cu coada si task propriu) pentru ca pe
// ATmega328P nu incape: taskul UART si cele doua cozi ale lui cer ~280 de
// octeti, iar Wire in mod sclav isi ia deja tampoanele proprii. Daca RAM-ul se
// termina, LED-ul de pe D13 clipeste rapid (vApplicationMallocFailedHook).
//
// Achizitia are prioritatea mai mare: jurnalul din taskul de publicare scrie
// sincron pe UART, deci nu are voie sa intarzie masuratorile.
void setup()
{
    uartStdioInit();
    printSlaveWelcome();

    if (!prepareSharedBuffer()) return;
    i2cSlaveLinkBegin();

    xTaskCreate(framePublishTask, "FRAME", PUBLISH_STACK, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(acquisitionTask, "ACQ", ACQUISITION_STACK, NULL, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();
}

void loop() { }
