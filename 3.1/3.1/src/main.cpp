#include <Arduino.h>
#include "uartStdio.h"
#include "ky013.h"
#include "signal.h"
constexpr uint16_t task1ffset = 0;
constexpr uint16_t task2ffset = 3;

constexpr uint16_t task1Rec = 10;
constexpr uint16_t task2Rec = 500;

ky013 sensor(A0);
Signal signals;
void vTask1GetData(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(task1ffset));
    TickType_t lastWake = xTaskGetTickCount();
    for (;;)
    {
        float tempC = sensor.readCelcius();
        SensorStatus st = (tempC < -55.0 || tempC > 125.0) ? SENSOR_ERROR_RANGE : SENSOR_OK;
        signals.signalUpdate(tempC, st);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(task1Rec));
    }
}
void vTask2Report(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(task2ffset));
    TickType_t lastWake = xTaskGetTickCount();
    for (;;)
    {
        Signal snap;
        signals.getSignals(&snap);
        float temp = snap.getValue();
        int whole = (int)temp;
        int frac = abs((int)(temp * 10)) % 10;
        printf("Temp: %d.%d C | Status: %s | Samples: %u\r\n",
        whole, frac, signalStatusStr(snap.getStatus()));
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(task2Rec));
    }
}

void setup() {
    rtosUartStdioInit();
    sensor.ky013Init();
    signals.signalInit();

    xTaskCreate(vTask1GetData, "Acquire", 128, NULL, 1, NULL);
    xTaskCreate(vTask2Report,  "Report",  128, NULL, 1, NULL);

    vTaskStartScheduler();
}

void loop() {

}
