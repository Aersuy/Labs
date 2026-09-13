#ifndef UART_STDIO_H
#define UART_STDIO_H
#include <Arduino.h>
#include <stdio.h>

void uart_stdio_init();
int uart_putchar(char c, FILE *stream);
int uart_getchar(FILE *stream);

#endif