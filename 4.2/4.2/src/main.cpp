#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "uartStdio.h"
#include "motorDc.h"

constexpr int MOTOR_IN1_PIN = 8;
constexpr int MOTOR_IN2_PIN = 7;
constexpr int MOTOR_ENABLE_PIN = 9;        

constexpr uint8_t CMD_BUF_LEN = 32;
constexpr uint8_t CMD_QUEUE_LEN = 8;
constexpr int REPORT_PERIOD_MS = 2000;
constexpr int QUEUE_SEND_TIMEOUT_MS = 50;

static MotorDc motor(MOTOR_IN1_PIN, MOTOR_IN2_PIN, MOTOR_ENABLE_PIN);

typedef enum {
    MOTOR_CMD_SET = 0,
    MOTOR_CMD_STOP,
    MOTOR_CMD_MAX,
    MOTOR_CMD_INC,
    MOTOR_CMD_DEC,
    MOTOR_CMD_STATUS
} MotorCommandType;


typedef struct {
    MotorCommandType type;
    int value;                
} MotorCommand;


typedef struct {
    int power;
    const char *direction;
    uint8_t duty;
} MotorState;

static QueueHandle_t xMotorQueue = NULL;
static QueueHandle_t xStateMailbox = NULL;   

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
      printf_P(PSTR("\b \b"));
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
    printf("  motor set <-100..100> - set signed power (sign = direction)\n");
    printf("  motor stop            - immediate stop\n");
    printf("  motor max             - 100%% in the current direction\n");
    printf("  motor inc             - power +10%%\n");
    printf("  motor dec             - power -10%%\n");
    printf("  motor status          - report the current state\n");
    printf("  help                  - this list\n");
}


static void motorTask(void *pvParameters)
{
    MotorCommand cmd;
    MotorState state;

    motor.begin();

    for (;;)
    {
        if (xQueueReceive(xMotorQueue, &cmd, portMAX_DELAY) == pdPASS)
        {
            bool ok = true;
            switch (cmd.type)
            {
                case MOTOR_CMD_SET:  ok = motor.setPower(cmd.value);   break;
                case MOTOR_CMD_STOP: motor.stop();                     break;
                case MOTOR_CMD_MAX:  motor.setMaxPower();              break;
                case MOTOR_CMD_INC:  ok = motor.increasePower();       break;
                case MOTOR_CMD_DEC:  ok = motor.decreasePower();       break;
                case MOTOR_CMD_STATUS: break;
            }

            if (!ok) printf("[motor] command refused (limit reached)\n");

            state.power = motor.getPower();
            state.direction = motor.directionName();
            state.duty = MotorDc::POWER_MAX ?
                         (uint8_t)((long)abs(state.power) * 255 / 100) : 0;

            xQueueOverwrite(xStateMailbox, &state);

            printf("[motor] dir=%s power=%d%%\n", state.direction, state.power);
        }
    }
}


static void cmdTask(void *pvParameters)
{
    char line[CMD_BUF_LEN];
    MotorCommand cmd;

    printf("\n=== Lab 4.2: DC motor control over L293D ===\n");
    printHelp();

    for (;;)
    {
        printf("> ");
        readLine(line, sizeof(line));
        format(line);

        if (line[0] == '\0') continue;

        int value = 0;
        if (sscanf(line, "motor set %d", &value) == 1)
        {
            if (value < MotorDc::POWER_MIN || value > MotorDc::POWER_MAX)
            {
                printf("[err] power out of range (%d..%d)\n",
                       MotorDc::POWER_MIN, MotorDc::POWER_MAX);
                continue;
            }
            cmd.type = MOTOR_CMD_SET;
            cmd.value = value;
        }
        else if (!strcmp(line, "motor stop"))   { cmd.type = MOTOR_CMD_STOP; }
        else if (!strcmp(line, "motor max"))    { cmd.type = MOTOR_CMD_MAX; }
        else if (!strcmp(line, "motor inc"))    { cmd.type = MOTOR_CMD_INC; }
        else if (!strcmp(line, "motor dec"))    { cmd.type = MOTOR_CMD_DEC; }
        else if (!strcmp(line, "motor status")) { cmd.type = MOTOR_CMD_STATUS; }
        else if (!strcmp(line, "help"))         { printHelp(); continue; }
        else
        {
            printf("[err] unknown command: \"%s\" (type help)\n", line);
            continue;
        }

        xQueueSend(xMotorQueue, &cmd, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
    }
}


static void reportTask(void *pvParameters)
{
    MotorState state;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(REPORT_PERIOD_MS));

        if (xQueuePeek(xStateMailbox, &state, 0) == pdPASS)
        {
            printf("\n[status] dir=%s power=%d%% pwm=%u/255\n> ",
                   state.direction, state.power, state.duty);
        }
    }
}

void setup()
{
    rtosUartStdioInit(9600, tskIDLE_PRIORITY + 1, 64);

    xMotorQueue = xQueueCreate(CMD_QUEUE_LEN, sizeof(MotorCommand));
    xStateMailbox = xQueueCreate(1, sizeof(MotorState));

    xTaskCreate(cmdTask,    "CMD",    320, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(motorTask,  "MOTOR",  256, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(reportTask, "REPORT", 192, NULL, tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();
}

void loop() { }