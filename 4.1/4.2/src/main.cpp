#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>
#include <ctype.h>
 
#include "uartStdio.h"
#include "binaryRelay.h"

constexpr int RELAY_PIN = 7;
constexpr uint8_t       RELAY_ACTIVE_LEVEL = LOW;    

constexpr int RELAY_MIN_INTERVAL = 300;     
constexpr uint8_t CMD_BUF_LEN = 32;

static Relay relay(RELAY_PIN, RELAY_ACTIVE_LEVEL, RELAY_MIN_INTERVAL);

typedef enum {
  RELAY_CMD_OFF = 0,
  RELAY_CMD_ON,
  RELAY_CMD_TOGGLE,
  RELAY_CMD_STATUS
} RelayCommand;

static QueueHandle_t xRelayQueue = NULL;

// read character 1 by 1 and block
// dont use fgets
static void readLine(char *buf, uint8_t len)
 {
  uint8_t i = 0;
  for (;;)
  {
    int ch = getchar();                      
    if (ch < 0) continue;

    if (ch == '\r' || ch == '\n') { putchar('\n'); break; }

    if ((ch == 0x7F || ch == '\b') && i > 0)  
    {
      i--;
      printf("\b \b");
      continue;
    }
    if (i < len - 1 && isprint(ch))
    {
      buf[i++] = (char)tolower(ch);
      putchar(ch);                           
    }
   }
  buf[i] = '\0';
}

// removes spaces also lowercases
static void format(char *s)
{
  char *w = s;
  bool space = false;
  for (char *r = s; *r; r++)
  {
    if (isspace((unsigned char)*r)) { space = (w != s); continue; }
    if (space) { *w++ = ' '; space = false; }
    *w++ = *r;
  }
  *w = '\0';
}

static void printHelp()
{
  printf("Commands:\n");
  printf("  relay on      - activate the actuator\n");
  printf("  relay off     - deactivate the actuator\n");
  printf("  relay toggle  - invert the state\n");
  printf("  relay status  - report the current state\n");
  printf("  help          - this list\n");
}
// Owns the Relay object: no other task touches the hardware
static void relayTask(void *pvParameters)
{
  RelayCommand cmd;
  relay.begin();

  for (;;)
  {
    if (xQueueReceive(xRelayQueue, &cmd, portMAX_DELAY) == pdPASS)
    {
      bool ok = true;
      switch (cmd)
      {
        case RELAY_CMD_ON:     ok = relay.turnOn();  break;
        case RELAY_CMD_OFF:    ok = relay.turnOff(); break;
        case RELAY_CMD_TOGGLE: ok = relay.toggle();  break;
        case RELAY_CMD_STATUS: break;                
      }

      if (!ok) printf("[relay] too soon, contact protection active\n");
      printf("[relay] state: %s\n", relay.stateName());
     }
   }
}


static void cmdTask(void *pvParameters)
 {
  char line[CMD_BUF_LEN];
  RelayCommand cmd;

  printf("\n=== Lab 4.1: binary actuator / relay control ===\n");
  printHelp();

  for (;;)
  {
    printf("> ");
    readLine(line, sizeof(line));
    format(line);

    if (line[0] == '\0') continue;                       // empty line

    if      (!strcmp(line, "relay on"))     cmd = RELAY_CMD_ON;
    else if (!strcmp(line, "relay off"))    cmd = RELAY_CMD_OFF;
    else if (!strcmp(line, "relay toggle")) cmd = RELAY_CMD_TOGGLE;
    else if (!strcmp(line, "relay status")) cmd = RELAY_CMD_STATUS;
    else if (!strcmp(line, "help"))         { printHelp(); continue; }
    else
    {
      printf("[err] unknown command: \"%s\" (type help)\n", line);
      continue;
    }

    xQueueSend(xRelayQueue, &cmd, pdMS_TO_TICKS(50));
   }
}

void setup()
{
  rtosUartStdioInit(9600, tskIDLE_PRIORITY + 1, 64);

  xRelayQueue = xQueueCreate(8, sizeof(RelayCommand));

  xTaskCreate(cmdTask,   "CMD",   320, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(relayTask, "RELAY", 256, NULL, tskIDLE_PRIORITY + 3, NULL);
  vTaskStartScheduler();
}

void loop() { }   