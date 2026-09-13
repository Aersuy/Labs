#include <Arduino.h>
#include "Button.h"
#include "ledClass.h"
#include <timer-api.h>
#include "uartStdio.h"
#define SRV_OS_SYS_TICK 1

#define TASK1_OFFSET 0
#define TASK1_REC 20
int task1_cnt = TASK1_OFFSET;

#define TASK2_OFFSET 5
#define TASK2_REC_INIT 200 // ms, Initial value
#define TASK2_REC_MIN 100 // ms
#define TASK2_REC_MAX 2000 // ms
#define TASK2_REC_STEP 50 // ms, Button increment/decrement size

int task2_cnt = TASK2_OFFSET;
volatile int task2Period = TASK2_REC_INIT; // providor Task 3, comsumer Task 2

#define TASK3_OFFSET 10
#define TASK3_REC 20
int task3_cnt = TASK3_OFFSET;

Button button1(8);
Button buttonInc(2);
Button buttonDesc(3);
Led led1(13);
Led led2(12);
void task1Led()
{
  button1.update();
  if (button1.wasPressed())
  {
    led1.toggle();
  }
}
void task2Led()
{
  if (led1.isOn())
  {
    led2.turnOff();
  } else
  {
    led2.toggle();
  }
}
void task3()
{
  buttonInc.update();
  buttonDesc.update();
  if (buttonInc.wasPressed() && (task2Period + TASK2_REC_STEP) <= TASK2_REC_MAX)
  {
    task2Period += TASK2_REC_STEP;
    task2_cnt = task2Period;
  }

  if (buttonDesc.wasPressed() && (task2Period - TASK2_REC_STEP) >= TASK2_REC_MIN)
  {
    task2Period -= TASK2_REC_STEP;
    task2_cnt = task2Period;
  }
  
}
void timer_handle_interrupts(int timer) 
{
  if (--task1_cnt <= 0)
  {
    task1_cnt = TASK1_REC;
    task1Led();
  }

    if (--task2_cnt <= 0)
  {
    task2_cnt = task2Period;
    task2Led();
  }
    if (--task3_cnt <= 0)
  {
    task3_cnt = TASK3_REC;
    task3();
  }
  
}
void setup() {
  button1.begin();
  buttonInc.begin();
  buttonDesc.begin();
  uart_stdio_init();
   //timer_init_ISR_500KHz(TIMER_DEFAULT);
    //timer_init_ISR_200KHz(TIMER_DEFAULT);
    //timer_init_ISR_100KHz(TIMER_DEFAULT);
    //timer_init_ISR_50KHz(TIMER_DEFAULT);
    //timer_init_ISR_20KHz(TIMER_DEFAULT);
    //timer_init_ISR_10KHz(TIMER_DEFAULT);
    //timer_init_ISR_5KHz(TIMER_DEFAULT);
    //timer_init_ISR_2KHz(TIMER_DEFAULT);
    timer_init_ISR_1KHz(TIMER_DEFAULT);
    //timer_init_ISR_500Hz(TIMER_DEFAULT);
    //timer_init_ISR_200Hz(TIMER_DEFAULT);
    //timer_init_ISR_100Hz(TIMER_DEFAULT);
    //timer_init_ISR_50Hz(TIMER_DEFAULT);
    //timer_init_ISR_20Hz(TIMER_DEFAULT);
    //timer_init_ISR_10Hz(TIMER_DEFAULT);
    //timer_init_ISR_5Hz(TIMER_DEFAULT);
    //timer_init_ISR_2Hz(TIMER_DEFAULT);
    //timer_init_ISR_1Hz(TIMER_DEFAULT);
}

void loop() {
  noInterrupts();
  int localPeriod = task2Period;
  interrupts();

  printf("LED1: %s | LED2: %s | LED2 period: %d ms\n",
       led1.isOn() ? "ON" : "OFF",
       led2.isOn() ? "ON" : "OFF",
       localPeriod);
  delay(1000);
}
