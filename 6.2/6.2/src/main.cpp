#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#include "uartStdio.h"
#include "Button.h"
#include "trafficHead.h"
#include "trafficFsm.h"

// Pinii 2..9 exista si pe Arduino Uno, deci montajul este identic pe Uno si
// pe Mega.
constexpr int EAST_RED_PIN = 2;
constexpr int EAST_YELLOW_PIN = 3;
constexpr int EAST_GREEN_PIN = 4;
constexpr int NORTH_RED_PIN = 5;
constexpr int NORTH_YELLOW_PIN = 6;
constexpr int NORTH_GREEN_PIN = 7;

// Cererea de trecere pentru Nord (buton pietonal sau senzor de prezenta) si
// comutatorul de mod noapte. Ambele sunt INPUT_PULLUP, deci un pin nelegat
// citeste HIGH: butonul de noapte poate lipsi din montaj fara efecte.
constexpr int NORTH_REQUEST_PIN = 8;
constexpr int NIGHT_SWITCH_PIN = 9;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 20;
constexpr TickType_t BUTTON_POLL_TICKS = 1;

constexpr UBaseType_t EVENT_QUEUE_LEN = 4;

constexpr uint16_t BUTTON_STACK = 144;
constexpr uint16_t FSM_STACK = 256;       // printf are nevoie de stiva
constexpr uint16_t HEADS_STACK = 128;

static TrafficHead eastHead(EAST_RED_PIN, EAST_YELLOW_PIN, EAST_GREEN_PIN);
static TrafficHead northHead(NORTH_RED_PIN, NORTH_YELLOW_PIN, NORTH_GREEN_PIN);

static QueueHandle_t xEventQueue = NULL;
static QueueHandle_t xPatternMailbox = NULL;

static void sendEvent(TrafficEvent event)
{
  if (xQueueSend(xEventQueue, &event, 0) != pdPASS)
  {
    printf_P(PSTR("[err] coada de evenimente plina, eveniment pierdut\n"));
  }
}

// Deține butoanele. Nu cunoaste automatul: traduce apasari in evenimente.
static void buttonTask(void *pvParameters)
{
  Button northRequest(NORTH_REQUEST_PIN, BUTTON_DEBOUNCE_MS);
  Button nightSwitch(NIGHT_SWITCH_PIN, BUTTON_DEBOUNCE_MS);

  northRequest.begin();
  nightSwitch.begin();
  TickType_t lastWake = xTaskGetTickCount();

  for (;;)
  {
    northRequest.update();
    nightSwitch.update();

    if (northRequest.wasPressed()) sendEvent(EVENT_NORTH_REQUEST);
    if (nightSwitch.wasPressed())  sendEvent(EVENT_NIGHT_TOGGLE);

    xTaskDelayUntil(&lastWake, BUTTON_POLL_TICKS);
  }
}

static void publishPattern(TrafficState state)
{
  SignalPattern pattern = trafficFsmPattern(state);
  xQueueOverwrite(xPatternMailbox, &pattern);
}

static void reportLamps(TrafficState state)
{
  SignalPattern pattern = trafficFsmPattern(state);

  printf_P(PSTR("       lampi: EST=%s NORD=%s%s\n"),
           lampColorName(pattern.east),
           lampColorName(pattern.north),
           pattern.blinkHalfPeriodMs == 0 ? "" : " (intermitent)");
}

static void reportTransition(TrafficState from, TrafficState to, TrafficEvent event)
{
  printf_P(PSTR("[fsm] %s -> %s (%s)\n"),
           trafficFsmStateName(from),
           trafficFsmStateName(to),
           trafficFsmEventName(event));
  reportLamps(to);
}

static void printWelcome()
{
  printf_P(PSTR("\n=== Lab 6.2: semafor inteligent cu automat finit (FreeRTOS) ===\n"));
  printf_P(PSTR("Est are prioritate: ramane verde cat timp nu exista cerere pe Nord\n"));
  printf_P(PSTR("Buton D8  : cerere Nord (servita dupa verdele minim de %u ms)\n"),
           EAST_MIN_GREEN_MS);
  printf_P(PSTR("Buton D9  : comuta modul noapte (galben intermitent)\n"));
  printf_P(PSTR("Automat   : mod noapte si dupa %u ms fara cereri\n"), EAST_HOLD_IDLE_MS);
}

// Cat mai are voie automatul sa stea in starea curenta. Expirarea asteptarii
// *este* evenimentul de faza, deci nu e nevoie de un cronometru separat. Se
// scade timpul deja scurs, altfel un eveniment ignorat (o cerere sosita in
// mijlocul galbenului) ar reporni faza de la zero.
static TickType_t remainingTicks(TrafficState state, TickType_t phaseStart)
{
  uint16_t phaseMs = trafficFsmPhaseMs(state);
  if (phaseMs == 0) return portMAX_DELAY;

  TickType_t phaseTicks = pdMS_TO_TICKS(phaseMs);
  TickType_t elapsed = xTaskGetTickCount() - phaseStart;
  if (elapsed >= phaseTicks) return 0;

  return phaseTicks - elapsed;
}

static TrafficEvent nextEvent(TrafficState state,
                              TickType_t phaseStart,
                              bool *isNorthRequested)
{
  if (*isNorthRequested && trafficFsmServesRequest(state))
  {
    return EVENT_NORTH_REQUEST;
  }

  TrafficEvent event;
  if (xQueueReceive(xEventQueue, &event, remainingTicks(state, phaseStart)) != pdPASS)
  {
    return EVENT_PHASE_TIMEOUT;
  }

  // O cerere sosita in mijlocul unei faze nu se pierde: ramane memorata pana
  // cand automatul ajunge intr-o stare care o poate servi.
  if (event == EVENT_NORTH_REQUEST) *isNorthRequested = true;
  return event;
}

static bool wasRequestServed(TrafficState state, TrafficEvent event)
{
  return event == EVENT_NORTH_REQUEST && trafficFsmServesRequest(state);
}

static void announceStart(TrafficState state)
{
  printWelcome();
  printf_P(PSTR("[fsm] stare initiala: %s\n"), trafficFsmStateName(state));
  reportLamps(state);
}

// Deține starea automatului si raportarea ei. Nu atinge nici butoanele, nici
// LED-urile: primeste evenimente si publica tiparul de ieșire. Lampile se
// publica inaintea jurnalului, ca scrierea pe UART sa nu intarzie culorile.
static void fsmTask(void *pvParameters)
{
  TrafficState state = EAST_GREEN_MIN;
  bool isNorthRequested = false;

  publishPattern(state);
  announceStart(state);
  TickType_t phaseStart = xTaskGetTickCount();

  for (;;)
  {
    TrafficEvent event = nextEvent(state, phaseStart, &isNorthRequested);
    TrafficState next = trafficFsmNext(state, event);

    if (next == state) continue;      // eveniment fara efect in starea curenta

    if (wasRequestServed(state, event)) isNorthRequested = false;

    TrafficState previous = state;
    state = next;
    phaseStart = xTaskGetTickCount();
    publishPattern(state);
    reportTransition(previous, state, event);
  }
}

static void showPattern(const SignalPattern *pattern, bool isFlashOff)
{
  eastHead.show(isFlashOff ? LAMP_OFF : pattern->east);
  northHead.show(isFlashOff ? LAMP_OFF : pattern->north);
}

// Deține cele doua capete de semafor. Asteptarea pe cutia poștala este si
// cronometrul de intermitenta: un tipar nou se aplica imediat, nu dupa
// semiperioada in curs.
static void headsTask(void *pvParameters)
{
  SignalPattern pattern = { LAMP_RED, LAMP_RED, 0 };
  bool isFlashOff = false;

  eastHead.begin();
  northHead.begin();

  for (;;)
  {
    TickType_t waitTicks = pattern.blinkHalfPeriodMs == 0
                              ? portMAX_DELAY
                              : pdMS_TO_TICKS(pattern.blinkHalfPeriodMs);

    if (xQueueReceive(xPatternMailbox, &pattern, waitTicks) == pdPASS)
    {
      isFlashOff = false;             // orice tipar nou porneste aprins
    }
    else
    {
      isFlashOff = !isFlashOff;
    }
    showPattern(&pattern, isFlashOff);
  }
}

// Varianta uartStdioInit() (nu cea cu coada si task propriu) pentru ca pe
// ATmega328P nu incape: taskul UART si cele doua cozi ale lui cer ~280 de
// octeti, iar FreeRTOS isi aloca deja taskul idle si cel de software timers.
// Daca RAM-ul se termina, LED-ul de pe D13 clipeste rapid
// (vApplicationMallocFailedHook din portul FreeRTOS).
//
// Butonul are prioritatea cea mai mare: printf-ul din taskul FSM scrie
// sincron pe UART, deci nu are voie sa intarzie citirea butoanelor.
void setup()
{
  uartStdioInit();

  xEventQueue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(TrafficEvent));
  xPatternMailbox = xQueueCreate(1, sizeof(SignalPattern));

  xTaskCreate(headsTask,  "HEADS",  HEADS_STACK,  NULL, tskIDLE_PRIORITY + 1, NULL);
  xTaskCreate(fsmTask,    "FSM",    FSM_STACK,    NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(buttonTask, "BUTTON", BUTTON_STACK, NULL, tskIDLE_PRIORITY + 3, NULL);

  vTaskStartScheduler();
}

void loop() { }
