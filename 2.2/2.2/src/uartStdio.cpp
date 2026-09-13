#include "uartStdio.h"

static FILE uartOut;
static FILE uartIn;


//Functie pentru redirectionarea stdout
int uartPutchar(char c, FILE *stream)
{
  Serial.write(c);
  return 0;
}
//Functie pentru redirectionarea stdin
int uartGetchar(FILE *stream)
{
  while(!Serial.available())
  {

  }
  return Serial.read();
}

//Functia ce efectueaza redirectionarea
void uartStdioInit()
{
    Serial.begin(9600);

    fdev_setup_stream(&uartOut, uartPutchar, NULL, _FDEV_SETUP_WRITE);
    fdev_setup_stream(&uartIn, NULL, uartGetchar, _FDEV_SETUP_READ);

    stdout = &uartOut;
    stdin = &uartIn;
}

/// RTOS FUNCTIONALITY
static FILE rtosUartStream;
static QueueHandle_t xUartTxQueue = NULL;
static QueueHandle_t xUartRxQueue = NULL;

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
int rtosUartPutchar(char c, FILE *stream)
{
  xQueueSend(xUartTxQueue,&c,pdMS_TO_TICKS(10));
  return 0;
}
int rtosUartGetchar(FILE *stream)
{
  char c;
  xQueueReceive(xUartRxQueue,&c,portMAX_DELAY);
  return c;
}