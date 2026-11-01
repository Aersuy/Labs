#ifndef UART_STDIO_H
#define UART_STDIO_H
#include <Arduino.h>
#include <stdio.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
void uartStdioInit();
int uartPutchar(char c, FILE *stream);
int uartGetchar(FILE *stream);

void rtosUartStdioInit(int baud = 9600,
                        UBaseType_t uartTaskPriority = tskIDLE_PRIORITY + 1,
                        UBaseType_t queueLength = 64);
int rtosUartPutchar(char c, FILE *stream);
int rtosUartGetchar(FILE *stream);

#endif