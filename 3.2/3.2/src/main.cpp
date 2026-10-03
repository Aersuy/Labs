#include <Arduino.h>
#include "uartStdio.h"
#include "ky013.h"
#include "ky018.h"
#include "signal.h"
#include "signalBuffer.h"
#include "conditioner.h"
constexpr uint16_t task1ffset = 0;
constexpr uint16_t task2ffset = 3;

constexpr uint16_t task1Rec = 10;
constexpr uint16_t task2Rec = 500;

ky013 tempSensor(A0);
ky018 lightSensor(A1);

SignalBuffer tempBuf(50);
SignalBuffer lightBuf(50);

Signal tempSignal;
Signal lightSignal;

Conditioner conditioner;
SemaphoreHandle_t reportReady;
void vTask1GetData(void *pvParameters)
{
   vTaskDelay(pdMS_TO_TICKS(task1ffset));
    TickType_t lastWake = xTaskGetTickCount();
    for (;;)
    {
        tempBuf.pushSample(tempSensor.readRaw());
        lightBuf.pushSample(lightSensor.readRaw());
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(task1Rec));
    }
}
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

        float condTemp = conditioner.condition(tempRaw, 50);
        float condLight = conditioner.condition(lightRaw, 50);

        float tempC = tempSensor.rawToCelsius(condTemp);
        SensorStatus st = (tempC < -55.0 || tempC > 125.0) ? SENSOR_ERROR_RANGE : SENSOR_OK;
        tempSignal.signalUpdate(tempC, st);

        float lightPct = (condLight / 1023.0) * 100.0;
        lightSignal.signalUpdate(lightPct, SENSOR_OK);

        xSemaphoreGive(reportReady);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(task2Rec));
    }
}
void vTask3Report(void *pvParameters)
{
    for (;;)
    {
        xSemaphoreTake(reportReady, portMAX_DELAY);
        Signal tSnap, lSnap;
        tempSignal.getSignals(&tSnap);
        lightSignal.getSignals(&lSnap);
        printf("Temp: %d C | Light: %d%% | Status: %s\r\n",
               (int)tSnap.getValue(), (int)lSnap.getValue(), signalStatusStr(tSnap.getStatus()));
    }
}

void setup() {
    reportReady = xSemaphoreCreateBinary();
    tempBuf.init();
    lightBuf.init();
    tempSignal.signalInit();
    lightSignal.signalInit();
    xTaskCreate(vTask1GetData,   "Acquire",   128, NULL, 1, NULL);
    xTaskCreate(vTask2Condition, "Condition", 384, NULL, 1, NULL);
    xTaskCreate(vTask3Report,    "Report",    128, NULL, 1, NULL);
    vTaskStartScheduler();
}

void loop() {

}
