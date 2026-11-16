#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>

#include "uartStdio.h"
#include "ledClass.h"
#include "Button.h"
#include "ledFsm.h"
#include "pressClassifier.h"

constexpr int LED_PIN = 13;
constexpr int BUTTON_PIN = 7;            // INPUT_PULLUP
constexpr unsigned long BUTTON_DEBOUNCE_MS = 20;


constexpr TickType_t BUTTON_POLL_TICKS = 1;

constexpr uint16_t DOUBLE_PRESS_WINDOW_MS = 400;

constexpr unsigned long UART_BAUD = 9600;
constexpr UBaseType_t UART_QUEUE_LEN = 64;
constexpr UBaseType_t EVENT_QUEUE_LEN = 4;

constexpr uint16_t BUTTON_STACK = 192;
constexpr uint16_t FSM_STACK = 256;       // needs stack
constexpr uint16_t LED_STACK = 128;

static Led led(LED_PIN);

static QueueHandle_t xEventQueue = NULL;
static QueueHandle_t xPatternMailbox = NULL;

static void sendEvent(LedFsmEvent event)
{
  if (xQueueSend(xEventQueue, &event, 0) != pdPASS)
  {  
    printf("[err] coada de evenimente plina, eveniment pierdut\n");
  }
}

static void publishPress(PressKind kind)
{
  if (kind == PRESS_SINGLE)      sendEvent(FSM_EVENT_SHORT_PRESS);
  else if (kind == PRESS_DOUBLE) sendEvent(FSM_EVENT_DOUBLE_PRESS);
}

static void buttonTask(void *pvParameters)
{
  Button button(BUTTON_PIN, BUTTON_DEBOUNCE_MS);
  PressClassifier classifier = { false, 0 };
  const uint16_t windowTicks = DOUBLE_PRESS_WINDOW_MS / portTICK_PERIOD_MS;

  button.begin();
  TickType_t lastWake = xTaskGetTickCount();

  for (;;)
  {
    button.update();

    PressClassification step =
      classifyPressTick(classifier, button.wasPressed(), windowTicks);
    classifier = step.next;
    publishPress(step.kind);

    xTaskDelayUntil(&lastWake, BUTTON_POLL_TICKS);
  }
}


static void publishPattern(LedFsmState state)
{
  LedPattern pattern = ledFsmPattern(state);
  xQueueOverwrite(xPatternMailbox, &pattern);
}

static void reportTransition(LedFsmState from, LedFsmState to, LedFsmEvent event)
{
  printf("[fsm] %s -> %s (%s) | LED: %s\n",
        ledFsmStateName(from),
        ledFsmStateName(to),
        ledFsmEventName(event),
        ledFsmPatternName(to));
}

static void printWelcome()
{
  printf("\n=== Lab 6.1: automat finit buton-LED (FreeRTOS) ===\n");
  printf("Stari: S0 OFF, S1 ON, S2 BLINK (~480 ms), S3 TIMEOUT_WARNING (~96 ms)\n");
  printf("Apasare simpla : S0<->S1, iar din S2/S3 revine in S0/S1\n");
  printf("Apasare dubla  : intra in S2 BLINK (din S2 revine in S1)\n");
  printf("Inactivitate   : S1 -> S3 dupa ~10 s, S3 -> S0 dupa ~3 s\n");
}

// Cat are voie automatul sa astepte in starea curenta. Expirarea asteptarii
// *este* evenimentul de inactivitate, deci nu e nevoie de un cronometru
// separat: orice apasare primita schimba starea si reporneste fereastra.
static TickType_t waitTicksFor(LedFsmState state)
{
  uint16_t timeoutMs = ledFsmTimeoutMs(state);
  if (timeoutMs == 0) return portMAX_DELAY;

  return pdMS_TO_TICKS(timeoutMs);
}

static LedFsmEvent nextEvent(LedFsmState state)
{
  LedFsmEvent event;

  if (xQueueReceive(xEventQueue, &event, waitTicksFor(state)) != pdPASS)
  {
    return FSM_EVENT_INACTIVITY;
  }
  return event;
}

 // Deține starea automatului si raportarea ei. Nu atinge nici butonul, nici
 // LED-ul: primeste evenimente si publica tiparul de ieșire.
static void fsmTask(void *pvParameters)
{
  LedFsmState state = FSM_OFF;

  printWelcome();
  publishPattern(state);
  printf("[fsm] stare initiala: %s | LED: %s\n",
        ledFsmStateName(state), ledFsmPatternName(state));

  for (;;)
  {
    LedFsmEvent event = nextEvent(state);
    LedFsmState next = ledFsmNext(state, event);

    if (next == state) continue;      // eveniment fara efect in starea curenta

    reportTransition(state, next, event);
    state = next;
    publishPattern(state);
  }
}

// Clipirea porneste mereu aprinsa, ca trecerea in S2/S3 sa se vada imediat.
static void applyPattern(const LedPattern *pattern)
{
  if (pattern->isBlinking || pattern->isOn) led.turnOn();
  else                                      led.turnOff();
}

 // Deține LED-ul. Asteptarea pe cutia poștala este si cronometrul de clipire:
 // un tipar nou se aplica imediat, nu dupa semiperioada in curs.
static void ledTask(void *pvParameters)
{
  LedPattern pattern = { false, false, 0 };

  led.begin();

  for (;;)
  {
    TickType_t waitTicks = pattern.isBlinking
                                ? pdMS_TO_TICKS(pattern.halfPeriodMs)
                                : portMAX_DELAY;

    if (xQueueReceive(xPatternMailbox, &pattern, waitTicks) == pdPASS)
    {
      applyPattern(&pattern);
      continue;
    }
    led.toggle();
  }
}

void setup()
{
  rtosUartStdioInit(UART_BAUD, tskIDLE_PRIORITY + 1, UART_QUEUE_LEN);

  xEventQueue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(LedFsmEvent));
  xPatternMailbox = xQueueCreate(1, sizeof(LedPattern));

  xTaskCreate(ledTask,    "LED",    LED_STACK,    NULL, tskIDLE_PRIORITY + 1, NULL);
  xTaskCreate(buttonTask, "BUTTON", BUTTON_STACK, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(fsmTask,    "FSM",    FSM_STACK,    NULL, tskIDLE_PRIORITY + 3, NULL);

  vTaskStartScheduler();
}

void loop() { }