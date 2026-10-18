#include <Arduino.h>
#include "uartStdio.h"
#include "signal.h"
#include "signalBuffer.h"
#include "conditioner.h"
#include "ky013.h"
#include "ky018.h"

// Create the offsets for the tasks
constexpr uint16_t task1ffset = 0;
constexpr uint16_t task2ffset = 3;

constexpr uint16_t task1Rec = 10;
constexpr uint16_t task2Rec = 500;



// create the sensor, buffer signal conditiner object
// and the semaphore handle
ky013 tempSensor(A0);
ky018 lightSensor(A1);


SignalBuffer tempBuf(50);
SignalBuffer lightBuf(50);


Signal tempSignal;
Signal tempRawSignal;
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
        tempBuf.pushSample(tempSensor.readRaw());
        lightBuf.pushSample(lightSensor.readRaw());

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(task1Rec));
    }
}
// Task 2 for conditioner
// gets the data from the buffer every 500 ms
// condition the data
// creating the signal object and setting the status
// giving the semaphore for the third task
void vTask2Condition(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(task2ffset));
    TickType_t lastWake = xTaskGetTickCount();
    int tempRaw[50];
    int lightRaw[50];
    for (;;)
    {
        tempBuf.copyOut(tempRaw, 50);
        lightBuf.copyOut(lightRaw, 50);

        // keep the most recent raw sample before the filter overwrites it in place
        int tempRawSample = tempRaw[49];
        int lightRawSample = lightRaw[49];

        conditioner.saltPepperFilter(tempRaw, 50);
        conditioner.saltPepperFilter(lightRaw, 50);

        float condTemp = conditioner.condition(tempRaw, 50);
        float condLight = conditioner.condition(lightRaw, 50);

        float tempC = tempSensor.rawToCelsius(condTemp);
        float tempRawC = tempSensor.rawToCelsius((float)tempRawSample);
        SensorStatus st = (tempC < -55.0 || tempC > 125.0) ? SENSOR_ERROR_RANGE : SENSOR_OK;
        tempSignal.signalUpdate(tempC, st);
        tempRawSignal.signalUpdate(tempRawC, st);

        // wired reversed: raw rises as it gets darker, so invert to get brightness %
        float lightPct = 100.0 - (condLight / 1023.0) * 100.0;
        float lightRawPct = 100.0 - ((float)lightRawSample / 1023.0) * 100.0;
        lightSignal.signalUpdate(lightPct, SENSOR_OK);
        lightRawSignal.signalUpdate(lightRawPct, SENSOR_OK);

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
        Signal tSnap, tRawSnap;
        Signal lSnap, lRawSnap;
        tempSignal.getSignals(&tSnap);
        tempRawSignal.getSignals(&tRawSnap);
        lightSignal.getSignals(&lSnap);
        lightRawSignal.getSignals(&lRawSnap);

        printf("Temp raw: %d C filt: %d C | Light raw: %d%% filt: %d%% | Status: %s\n",
               (int)tRawSnap.getValue(), (int)tSnap.getValue(),
               (int)lRawSnap.getValue(), (int)lSnap.getValue(),
               signalStatusStr(tSnap.getStatus()));
    }
}

void setup() {
    // init all of the objects
    reportReady = xSemaphoreCreateBinary();
    tempBuf.init();
    lightBuf.init();
    tempSignal.signalInit();
    tempRawSignal.signalInit();
    lightSignal.signalInit();
    lightRawSignal.signalInit();
    rtosUartStdioInit();

    // create the tasks
    xTaskCreate(vTask1GetData,   "Acquire",   128, NULL, 1, NULL);
    xTaskCreate(vTask2Condition, "Condition", 640, NULL, 1, NULL);
    xTaskCreate(vTask3Report,    "Report",    320, NULL, 1, NULL);

    //start the task scheduler
    vTaskStartScheduler();
}

void loop() {
}
