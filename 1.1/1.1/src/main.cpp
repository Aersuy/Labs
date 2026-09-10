#include <Arduino.h>
#include "uartStdio.h"
#include "ledClass.h"
// Created the led
Led led(12);
// Redirected the stream and wrote the starting message
void setup() {
  uart_stdio_init();
  printf("Initializat\n");
  printf("Comenzi disponibile: led on / led off\n");
}
// Took in from keyboard
// Format and remove any unneded characters
// Turn led on/off
void loop() {
  char comanda[20];
  scanf("%19[^\r\n]",comanda);
  int c;
  while ((c = getchar()) != '\n' && c != '\r' && c != EOF)
  {
  }
  int len = strlen(comanda);
  if (len > 0 && (comanda[len-1] == '\r' || comanda[len-1] == '\n'))
  {
    comanda[len-1] = '\0';
  }
  for (int j = 0; comanda[j]; j++)
  {
    comanda[j] = tolower(comanda[j]);
  }
  if(strcmp(comanda,"led on") == 0)
  {
    led.turnOn();
    printf("Led pornit \n");
  }
  else if(strcmp(comanda,"led off") == 0)
  {
    led.turnOff();
    printf("Led oprit\n");
  }
  else {
    printf("Comanda invalida\n");
  }
}
