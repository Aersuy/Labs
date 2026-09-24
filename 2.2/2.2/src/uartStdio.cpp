#include "uartStdio.h"

static FILE uartOut;
static FILE uartIn;


// Callback for stdout
int uartPutchar(char c, FILE *stream)
{
  Serial.write(c);
  return 0;
}
// Callback for stdin
int uartGetchar(FILE *stream)
{
  while(!Serial.available())
  {

  }
  return Serial.read();
}

// function that does the redirect
void uartStdioInit()
{
    Serial.begin(9600);

    fdev_setup_stream(&uartOut, uartPutchar, NULL, _FDEV_SETUP_WRITE);
    fdev_setup_stream(&uartIn, NULL, uartGetchar, _FDEV_SETUP_READ);

    stdout = &uartOut;
    stdin = &uartIn;
}

/// RTOS Object names
static FILE rtosUartStream;
static QueueHandle_t xUartTxQueue = NULL;
static QueueHandle_t xUartRxQueue = NULL;

// The output task that reads from the Tx Queue
// and sends to the Rx queue
static void uartTask(void *pvParameters)
{
  char c;
  for(;;)
  {
    while (xQueueReceive(xUartTxQueue,&c,0))
    {
      if (c == '\n') Serial.write('\r');
      Serial.write(c);
    }
    while (Serial.available())
    {
      char rc = Serial.read();
      xQueueSend(xUartRxQueue,&rc,0);
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}
// the main init for the stdio redirects for RTOS
// it creates the 2 queue's, redirects stdout and creates 
// the task
void rtosUartStdioInit(int baud,
                        UBaseType_t uartTaskPriority,
                        UBaseType_t queueLength)
{
  Serial.begin(baud);

  xUartTxQueue = xQueueCreate(queueLength,sizeof(char));
  xUartRxQueue = xQueueCreate(queueLength,sizeof(char));

  fdev_setup_stream(&rtosUartStream, rtosUartPutchar, rtosUartGetchar, _FDEV_SETUP_RW);
  stdout = &rtosUartStream;
  stdin  = &rtosUartStream;
   xTaskCreate(uartTask, "UART", 128, NULL, uartTaskPriority, NULL);
}
// the RTOS callback that sends to the Tx Queue
int rtosUartPutchar(char c, FILE *stream)
{
  xQueueSend(xUartTxQueue,&c,pdMS_TO_TICKS(10));
  return 0;
}
// the RTOS callaback that takes from the RxQueue
int rtosUartGetchar(FILE *stream)
{
  char c;
  xQueueReceive(xUartRxQueue,&c,portMAX_DELAY);
  return c;
}