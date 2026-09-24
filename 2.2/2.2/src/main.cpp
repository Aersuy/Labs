#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include "uartStdio.h"
#include "ledClass.h"
#include "Button.h"
#include <semphr.h>

// Defined all timing variables
// as compile time constants
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


// Defined all other globals
SemaphoreHandle_t xBinSem;
QueueHandle_t xByteQueue;
Led led(13);
Led led2(12);
volatile bool ledBusy = false;

//
// Runs periodically with a recurrence time of task1Rec ms, after an
// initial start-up offset of task1Offset ms.
//
// Each cycle it polls the button on pin 7. On a press, it turns the LED
// on for task1LedHold ms and then turns it off. When the pulse ends, it
// signals another task through the binary semaphore xBinSem.
//
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
//
// Task 2 – Event-driven sequence generator and LED blinker
//
// This task isn't periodic. It stays blocked until Task 1 gives the
// binary semaphore xBinSem, which happens at the end of each LED pulse.
//
//   Proccess
//   1. Increments the counter N (1st event -> N=1, 2nd -> N=2, ...)
//   2. Sends the numbers 1..N to xByteQueue, one every task2Rec ms,
//      followed by a 0 that marks the end of the sequence
//   3. Blinks the LED N times
//   4. Clears ledBusy so Task 1 can accept button presses again
//
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
        led2.turnOn();
        vTaskDelay(pdMS_TO_TICKS(task2LedOn));
        led2.turnOff();
        vTaskDelay(pdMS_TO_TICKS(task2LedOff));
      }
      ledBusy = false;
    }
  }  
}
// Runs periodically with a recurrence time of task3Rec ms, after an
// initial start-up offset of task3Offset ms.
//
// On each run it empties xByteQueue, which Task 2 fills, and prints
// the values. A 0 marks the end of a sequence and is printed as a
// new line, so each of Task 2's sequences appears on its own line:
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

// Initialises the hardware and creates the RTOS objects the tasks use
// to communicate: a binary semaphore (Task 1 -> Task 2) and a byte
// queue (Task 2 -> Task 3). It then creates the three tasks and starts
// the FreeRTOS scheduler.
//
void setup() {

  rtosUartStdioInit();
  led.begin();
  led2.begin();

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

