#ifndef UART_STDIO_H
#define UART_STDIO_H
#include <Arduino.h>
#include <stdio.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
void uartStdioInit();
int uartPutchar(char c, FILE *stream);
int uartGetchar(FILE *stream);

// Serialises a whole line of output against the other tasks. Characters reach
// the UART through one shared queue, so without this a status line and the
// command echo interleave character by character. Recursive, so a message
// built from several printf() calls can nest helpers that also lock.
void uartStdioLock();
void uartStdioUnlock();

void rtosUartStdioInit(int baud = 9600,
                        UBaseType_t uartTaskPriority = tskIDLE_PRIORITY + 1,
                        UBaseType_t queueLength = 64);
int rtosUartPutchar(char c, FILE *stream);
int rtosUartGetchar(FILE *stream);

#endif