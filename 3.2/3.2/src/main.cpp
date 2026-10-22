#include <Arduino.h>
#include "uartStdio.h"
#include "signal.h"
#include "signalBuffer.h"
#include "conditioner.h"
#include "ky018.h"

// Create the offsets for the tasks
constexpr uint16_t task1ffset = 0;
constexpr uint16_t task2ffset = 3;

constexpr uint16_t task1Rec = 10;
constexpr uint16_t task2Rec = 500;

// create the sensor, buffer, signal conditioner object
// and the semaphore handle
ky018 lightSensor(A1);

SignalBuffer lightBuf(50);

Signal lightSignal;
Signal lightRawSignal;

Conditioner conditioner;
SemaphoreHandle_t reportReady;

// Creating the first task to get the data
void vTask1GetData(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(task1ffset));
    TickType_t lastWake = xTaskGetTickCount();
    for (;;)
    {
        // getting the raw sensor data and adding it to
        // the buffer
        lightBuf.pushSample(lightSensor.readRaw());

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(task1Rec));
    }
}

// Task 2 for conditioner
// gets the data from the buffer every 500 ms
// conditions the data with the weighted average
// creating the signal object and setting the status
// giving the semaphore for the third task
void vTask2Condition(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(task2ffset));
    TickType_t lastWake = xTaskGetTickCount();
    int lightRaw[50];
    for (;;)
    {
        lightBuf.copyOut(lightRaw, 50);

        // the newest unfiltered sample, for the raw column of the report
        int lightRawSample = lightRaw[49];

        // weighted average only
        float condLight = conditioner.condition(lightRaw, 50);

        // wired reversed: raw rises as it gets darker, so invert to get brightness %
        float lightPct = 100.0 - (condLight / 1023.0) * 100.0;
        float lightRawPct = 100.0 - ((float)lightRawSample / 1023.0) * 100.0;

        SensorStatus st = (lightPct < 0.0 || lightPct > 100.0) ? SENSOR_ERROR_RANGE : SENSOR_OK;
        lightSignal.signalUpdate(lightPct, st);
        lightRawSignal.signalUpdate(lightRawPct, st);

        xSemaphoreGive(reportReady);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(task2Rec));
    }
}

// third task for the report
// gets the signals and prints it
void vTask3Report(void *pvParameters)
{
    for (;;)
    {
        xSemaphoreTake(reportReady, portMAX_DELAY);
        Signal lSnap, lRawSnap;
        lightSignal.getSignals(&lSnap);
        lightRawSignal.getSignals(&lRawSnap);

        printf("Light raw: %d%% filt: %d%% | Status: %s\n",
               (int)lRawSnap.getValue(), (int)lSnap.getValue(),
               signalStatusStr(lSnap.getStatus()));
    }
}

void setup() {
    // init all of the objects
    reportReady = xSemaphoreCreateBinary();
    lightBuf.init();
    lightSignal.signalInit();
    lightRawSignal.signalInit();
    rtosUartStdioInit();

    // create the tasks
    xTaskCreate(vTask1GetData,   "Acquire",   128, NULL, 1, NULL);
    xTaskCreate(vTask2Condition, "Condition", 384, NULL, 1, NULL);
    xTaskCreate(vTask3Report,    "Report",    256, NULL, 1, NULL);

    //start the task scheduler
    vTaskStartScheduler();
}

void loop() {
}
