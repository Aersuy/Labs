#include "uartStdio.h"

static FILE uartOut;
static FILE uartIn;


//Functie pentru redirectionarea stdout
int uart_putchar(char c, FILE *stream)
{
  Serial.write(c);
  return 0;
}
//Functie pentru redirectionarea stdin
int uart_getchar(FILE *stream)
{
  while(!Serial.available())
  {

  }
  return Serial.read();
}
//Functia ce efectueaza redirectionarea
void uart_stdio_init()
{
    Serial.begin(9600);
    fdev_setup_stream(&uartOut, uart_putchar, NULL, _FDEV_SETUP_WRITE);
    fdev_setup_stream(&uartIn, NULL, uart_getchar, _FDEV_SETUP_READ);
    stdout = &uartOut;
    stdin = &uartIn;
}
