#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include "uartStdio.h"
#include "ledClass.h"
#include "Button.h"
#include <semphr.h>

#define TASK1_OFFSET 0
#define TASK2_OFFSET 3
#define TASK3_OFFSET 8

#define TASK1_REC 10
#define TASK2_REC 50
#define TASK2_REC_LED_ON 300
#define TASK2_REC_LED_OFF 500
#define TASK3_REC 200

SemaphoreHandle_t xBinSem;
QueueHandle_t xByteQueue;
Led led(13);
volatile bool ledBusy = false;
void vTask1Led(void *pvParameters)
{
  vTaskDelay(pdMS_TO_TICKS(TASK1_OFFSET));
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t period  = pdMS_TO_TICKS(TASK1_REC);
  
  Button button(7,10); // pin 7, 10 ms debounce to match task
  button.begin();

  unsigned long ledTimer = 0;
  for (;;)
  { 
    button.update();
    if (button.wasPressed() && !led.isOn() && !ledBusy)
    {
      led.turnOn();
      ledTimer = millis();
    }
    if(led.isOn() && (millis() - ledTimer) >= 1000)
    {
      led.turnOff();
      xSemaphoreGive(xBinSem);
    }
    xTaskDelayUntil(&lastWakeTime,period);
  }  
}
void vTask2(void *pvParameters)
{
  vTaskDelay(pdMS_TO_TICKS(TASK2_OFFSET));
  int N = 0;
  for (;;)
  { 
    if (xSemaphoreTake(xBinSem,portMAX_DELAY) == pdTRUE)
    {
      ledBusy = true;
      N += 1;
      for (uint8_t i = 1; i <= N; i++)
      {
        xQueueSendToFront(xByteQueue, &i,portMAX_DELAY );
        vTaskDelay(pdMS_TO_TICKS(50));
      }
      uint8_t terminator = 0;
      xQueueSendToFront(xByteQueue, &terminator, portMAX_DELAY);
      for (int i = 0; i < N; i++)
      {
        led.turnOn();
        vTaskDelay(pdMS_TO_TICKS(TASK2_REC_LED_ON));
        led.turnOff();
        vTaskDelay(pdMS_TO_TICKS(TASK2_REC_LED_OFF));
      }
      ledBusy = false;
    }
  }  
}
void vTask3(void *pvParameters)
{
  vTaskDelay(pdMS_TO_TICKS(TASK3_OFFSET));
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t period  = pdMS_TO_TICKS(TASK3_REC);
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
  
  xTaskCreate(vTask1Led,"Task1",128,NULL,1,NULL);
  xTaskCreate(vTask2,"Task2",128,NULL,1,NULL);
  xTaskCreate(vTask3,"Task3",128,NULL,1,NULL);

  vTaskStartScheduler();
}

void loop() {
 
}

