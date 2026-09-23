#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include "uartStdio.h"
#include "ledClass.h"
#include "Button.h"
#include <semphr.h>

constexpr uint16_t task1Offset = 0;
constexpr uint16_t task3Offset = 8;

constexpr uint16_t task1Rec = 10;
constexpr uint16_t task2Rec = 50;
constexpr uint16_t task2LedOn = 300;
constexpr uint16_t task2LedOff = 500;
constexpr uint16_t task3Rec = 200;

constexpr uint16_t buttonDebounceTime = 10;
constexpr uint16_t task1LedHold = 1000;
constexpr uint16_t taskStackSize = 128;

SemaphoreHandle_t xBinSem;
QueueHandle_t xByteQueue;
Led led(13);
volatile bool ledBusy = false;


void vTask1Led(void *pvParameters)
{
  vTaskDelay(pdMS_TO_TICKS(task1Offset));
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t period  = pdMS_TO_TICKS(task1Rec);
  
  Button button(7,buttonDebounceTime);
  button.begin();

  unsigned long ledTimer = 0;
  bool waitingForOff = false; 
  for (;;)
  { 
    button.update();
    if (button.wasPressed() && !waitingForOff && !ledBusy)
    {
      led.turnOn();
      ledTimer = millis();
      waitingForOff = true;
    }
    if(waitingForOff && (millis() - ledTimer) >= task1LedHold)
    {
      led.turnOff();
      waitingForOff = false;
      ledBusy = true;
      xSemaphoreGive(xBinSem);
    }
    xTaskDelayUntil(&lastWakeTime,period);
  }  
}
void vTask2(void *pvParameters)
{
  int N = 0;
  for (;;)
  { 
    if (xSemaphoreTake(xBinSem,portMAX_DELAY) == pdTRUE)
    {
      N += 1;
      for (uint8_t i = 1; i <= N; i++)
      {
        xQueueSendToBack(xByteQueue, &i,portMAX_DELAY );
        vTaskDelay(pdMS_TO_TICKS(task2Rec));
      }
      uint8_t terminator = 0;
      xQueueSendToBack(xByteQueue, &terminator, portMAX_DELAY);
      for (int i = 0; i < N; i++)
      {
        led.turnOn();
        vTaskDelay(pdMS_TO_TICKS(task2LedOn));
        led.turnOff();
        vTaskDelay(pdMS_TO_TICKS(task2LedOff));
      }
      ledBusy = false;
    }
  }  
}
void vTask3(void *pvParameters)
{
  vTaskDelay(pdMS_TO_TICKS(task3Offset));
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t period  = pdMS_TO_TICKS(task3Rec);
  for (;;)
  {
    uint8_t b;
    while (xQueueReceive(xByteQueue, &b, 0) == pdTRUE)
    {
      if (b == 0)
      {
        printf("\n");
        continue;
      }
      printf("%d ",b);
    }
    xTaskDelayUntil(&lastWakeTime,period);
  }  
}


void setup() {

  rtosUartStdioInit();
  led.begin();

  xBinSem = xSemaphoreCreateBinary();
  xByteQueue = xQueueCreate(20,sizeof(uint8_t));

  if (xBinSem == NULL || xByteQueue == NULL)
  {
    printf("Something went wrong");
  }
  
  xTaskCreate(vTask1Led,"Task1",taskStackSize,NULL,1,NULL);
  xTaskCreate(vTask2,"Task2",taskStackSize,NULL,1,NULL);
  xTaskCreate(vTask3,"Task3",taskStackSize,NULL,1,NULL);
  vTaskStartScheduler();
}

void loop() {
}

