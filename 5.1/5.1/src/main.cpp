#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "uartStdio.h"
#include "binaryRelay.h"
#include "ledClass.h"
#include "dhtSensor.h"
#include "hysteresisController.h"
#include "deciTemperature.h"
#include "controlState.h"

constexpr int DHT_DATA_PIN = 2;
constexpr int RELAY_PIN = 7;
constexpr uint8_t RELAY_ACTIVE_LEVEL = LOW;
constexpr unsigned long RELAY_MIN_SWITCH_INTERVAL_MS = 1000;
constexpr int ALARM_LED_PIN = 12;

constexpr int16_t DEFAULT_SET_POINT_DECI_C = 250;      // 25.0 C
constexpr int16_t DEFAULT_HYSTERESIS_DECI_C = 10;      // 1.0 C
constexpr int16_t DEVIATION_ALARM_DECI_C = 30;         // 3.0 C

constexpr uint16_t SENSOR_PERIOD_MS = DhtSensor::MIN_READ_INTERVAL_MS;
constexpr uint16_t REPORT_PERIOD_MS = 500;
constexpr uint16_t ALARM_PERIOD_MS = 200;

constexpr uint16_t SENSOR_START_OFFSET_MS = 3;
constexpr uint16_t REPORT_START_OFFSET_MS = 7;
constexpr uint16_t ALARM_START_OFFSET_MS = 11;

constexpr uint8_t CMD_BUF_LEN = 32;
constexpr uint8_t CMD_ARG_LEN = 12;
constexpr uint8_t CONTROL_QUEUE_LEN = 8;
constexpr int QUEUE_SEND_TIMEOUT_MS = 50;
constexpr int PLOT_OUTPUT_SCALE = 10;

constexpr int UART_BAUD = 9600;
constexpr UBaseType_t UART_QUEUE_LEN = 64;

static DhtSensor dht(DHT_DATA_PIN);
static Relay relay(RELAY_PIN, RELAY_ACTIVE_LEVEL, RELAY_MIN_SWITCH_INTERVAL_MS);
static Led alarmLed(ALARM_LED_PIN);
static HysteresisController controller(DEFAULT_SET_POINT_DECI_C,
                                       DEFAULT_HYSTERESIS_DECI_C,
                                       CONTROL_MODE_HEATING);

static QueueHandle_t xControlQueue = NULL;
static QueueHandle_t xStateMailbox = NULL;

// atomic no queue needed
static volatile bool isPlotEnabled = false;


static void printDeci(int16_t value)
{
    char text[DECI_TEXT_MIN_LEN];
    formatDeciCelsius(value, text, sizeof(text));
    printf("%s", text);
}

static void printRejectedRange(const char *name, int16_t minimum, int16_t maximum)
{
    printf("[err] %s in afara domeniului (", name);
    printDeci(minimum);
    printf(" .. ");
    printDeci(maximum);
    printf(")\n");
}

static void printStatusLine(const ControlState *state)
{
    printf("[status] T=");
    printDeci(state->measuredDeciC);
    printf("C RH=");
    printDeci(state->humidityDeciPercent);
    printf("%% | SP=");
    printDeci(state->setPointDeciC);
    printf("C hyst=");
    printDeci(state->hysteresisDeciC);
    printf("C | praguri ");
    printDeci(state->lowerThresholdDeciC);
    printf("..");
    printDeci(state->upperThresholdDeciC);
    printf(" | mod=%s iesire=%s | comutari=%u | senzor=%s\n",
           controlModeName(state->mode),
           controlOutputName(state->output),
           state->switchCount,
           dhtReadStatusName(state->sensorStatus));
}

static void printPlotLine(const ControlState *state)
{
    printf("T:");
    printDeci(state->measuredDeciC);
    printf(" SP:");
    printDeci(state->setPointDeciC);
    printf(" OUT:%d\n", (int)state->output * PLOT_OUTPUT_SCALE);
}

static void printHelp()
{
    printf("Comenzi:\n");
    printf("  sp <val>        - set point, ex. sp 25.5\n");
    printf("  hyst <val>      - histereza, ex. hyst 1.5 (min 1.0)\n");
    printf("  mode heat|cool  - sensul actionarii\n");
    printf("  status          - raport imediat\n");
    printf("  plot on|off     - linii pentru Serial Plotter\n");
    printf("  help            - lista de comenzi\n");
}

static ControlState buildInitialState()
{
    ControlState state;

    state.measuredDeciC = 0;
    state.humidityDeciPercent = 0;
    state.setPointDeciC = controller.getSetPointDeciC();
    state.hysteresisDeciC = controller.getHysteresisDeciC();
    state.lowerThresholdDeciC = controller.lowerThresholdDeciC();
    state.upperThresholdDeciC = controller.upperThresholdDeciC();
    state.output = controller.getOutput();
    state.mode = controller.getMode();
    state.sensorStatus = DHT_READ_FAILED;   // nothing measured yet
    state.switchCount = 0;
    return state;
}

static void refreshControllerFields(ControlState *state)
{
    state->setPointDeciC = controller.getSetPointDeciC();
    state->hysteresisDeciC = controller.getHysteresisDeciC();
    state->lowerThresholdDeciC = controller.lowerThresholdDeciC();
    state->upperThresholdDeciC = controller.upperThresholdDeciC();
    state->mode = controller.getMode();
}

// Applies the decision to the hardware and counts the transitions that really
// reached the relay. A refusal comes from the contact-protection interval and
// is reported instead of being swallowed.
static void applyControlOutput(ControlOutput output, uint16_t *switchCount)
{
    bool wasOn = relay.isOn();
    bool isAccepted = (output == CONTROL_OUTPUT_ON) ? relay.turnOn()
                                                    : relay.turnOff();
    if (!isAccepted)
    {
        printf("[releu] comutare refuzata, protectie contacte activa\n");
        return;
    }
    if (relay.isOn() != wasOn) (*switchCount)++;
}

// Fail-safe: a reading we cannot trust forces the actuator OFF.
static void applyFailedSample(const DhtSample *sample, ControlState *state)
{
    state->output = CONTROL_OUTPUT_OFF;
    applyControlOutput(CONTROL_OUTPUT_OFF, &state->switchCount);
    printf("[senzor] citire invalida (%s), releu fortat OFF\n",
           dhtReadStatusName(sample->status));
}

static void applyMeasurement(const DhtSample *sample, ControlState *state)
{
    state->sensorStatus = sample->status;

    if (sample->status != DHT_READ_OK)
    {
        applyFailedSample(sample, state);
        return;
    }

    state->measuredDeciC = sample->temperatureDeciC;
    state->humidityDeciPercent = sample->humidityDeciPercent;
    state->output = controller.update(sample->temperatureDeciC);
    applyControlOutput(state->output, &state->switchCount);
}

static void applySetPoint(int16_t setPointDeciC, ControlState *state)
{
    if (!controller.setSetPoint(setPointDeciC))
    {
        printRejectedRange("set point",
                           HysteresisController::SET_POINT_MIN_DECI_C,
                           HysteresisController::SET_POINT_MAX_DECI_C);
        return;
    }
    refreshControllerFields(state);
    printf("[control] set point nou: ");
    printDeci(state->setPointDeciC);
    printf("C, praguri ");
    printDeci(state->lowerThresholdDeciC);
    printf("..");
    printDeci(state->upperThresholdDeciC);
    printf("\n");
}

static void applyHysteresis(int16_t hysteresisDeciC, ControlState *state)
{
    if (!controller.setHysteresis(hysteresisDeciC))
    {
        printRejectedRange("histereza",
                           HysteresisController::HYSTERESIS_MIN_DECI_C,
                           HysteresisController::HYSTERESIS_MAX_DECI_C);
        return;
    }
    refreshControllerFields(state);
    printf("[control] histereza noua: ");
    printDeci(state->hysteresisDeciC);
    printf("C\n");
}

static void applyMode(ControlMode mode, ControlState *state)
{
    controller.setMode(mode);
    refreshControllerFields(state);
    printf("[control] mod nou: %s\n", controlModeName(state->mode));
}

static void applyControlCommand(const ControlCommand *command, ControlState *state)
{
    switch (command->type)
    {
        case CONTROL_CMD_MEASUREMENT:
            applyMeasurement(&command->payload.sample, state);
            break;
        case CONTROL_CMD_SET_POINT:
            applySetPoint(command->payload.value, state);
            break;
        case CONTROL_CMD_HYSTERESIS:
            applyHysteresis(command->payload.value, state);
            break;
        case CONTROL_CMD_MODE:
            applyMode((ControlMode)command->payload.value, state);
            break;
        case CONTROL_CMD_STATUS:
            printStatusLine(state);
            break;
    }
}

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

// Collapses runs of whitespace so "sp   25.5" parses like "sp 25.5".
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

static bool hasCommandPrefix(const char *line, const char *prefix)
{
    size_t length = strlen(prefix);
    return strncmp(line, prefix, length) == 0 && line[length] == ' ';
}


static bool sendValueCommand(ControlCommandType type, int16_t value)
{
    ControlCommand command;
    command.type = type;
    command.payload.value = value;
    return xQueueSend(xControlQueue, &command,
                      pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS)) == pdPASS;
}

// Validates the numeric argument right where it enters the system, so the
// control task only ever sees well-formed tenths.
static bool handleDeciCommand(const char *line, const char *prefix,
                              ControlCommandType type)
{
    if (!hasCommandPrefix(line, prefix)) return false;

    const char *argument = line + strlen(prefix) + 1;
    int16_t value = 0;
    if (!parseDeciCelsius(argument, &value))
    {
        printf("[err] valoare invalida: \"%s\"\n", argument);
        return true;
    }
    sendValueCommand(type, value);
    return true;
}

static bool handleModeCommand(const char *line)
{
    if (!hasCommandPrefix(line, "mode")) return false;

    const char *argument = line + strlen("mode") + 1;
    if (!strcmp(argument, "heat"))
    {
        sendValueCommand(CONTROL_CMD_MODE, (int16_t)CONTROL_MODE_HEATING);
    }
    else if (!strcmp(argument, "cool"))
    {
        sendValueCommand(CONTROL_CMD_MODE, (int16_t)CONTROL_MODE_COOLING);
    }
    else
    {
        printf("[err] mod necunoscut: \"%s\" (heat|cool)\n", argument);
    }
    return true;
}

static bool handlePlotCommand(const char *line)
{
    if (!hasCommandPrefix(line, "plot")) return false;

    const char *argument = line + strlen("plot") + 1;
    if (!strcmp(argument, "on"))       isPlotEnabled = true;
    else if (!strcmp(argument, "off")) isPlotEnabled = false;
    else
    {
        printf("[err] argument necunoscut: \"%s\" (on|off)\n", argument);
        return true;
    }
    printf("[plot] %s\n", isPlotEnabled ? "activat" : "dezactivat");
    return true;
}

static void dispatchCommand(const char *line)
{
    if (handleDeciCommand(line, "sp", CONTROL_CMD_SET_POINT)) return;
    if (handleDeciCommand(line, "hyst", CONTROL_CMD_HYSTERESIS)) return;
    if (handleModeCommand(line)) return;
    if (handlePlotCommand(line)) return;

    if (!strcmp(line, "status"))
    {
        sendValueCommand(CONTROL_CMD_STATUS, 0);
        return;
    }
    if (!strcmp(line, "help"))
    {
        printHelp();
        return;
    }
    printf("[err] comanda necunoscuta: \"%s\" (scrie help)\n", line);
}



// Owns the sensor: the only task that talks to the DHT11.
static void sensorTask(void *pvParameters)
{
    ControlCommand command;
    command.type = CONTROL_CMD_MEASUREMENT;

    dht.begin();
    vTaskDelay(pdMS_TO_TICKS(SENSOR_START_OFFSET_MS));
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        command.payload.sample = dht.read();
        xQueueSend(xControlQueue, &command, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

// Owns the relay and the controller: no other task touches either.
static void controlTask(void *pvParameters)
{
    ControlCommand command;
    ControlState state = buildInitialState();

    relay.begin();
    controlStatePublish(xStateMailbox, &state);

    for (;;)
    {
        if (xQueueReceive(xControlQueue, &command, portMAX_DELAY) != pdPASS) continue;

        applyControlCommand(&command, &state);
        controlStatePublish(xStateMailbox, &state);
    }
}

static void cmdTask(void *pvParameters)
{
    char line[CMD_BUF_LEN];

    printf("\n=== Lab 5.1: control ON-OFF cu histereza (DHT11 + releu) ===\n");
    printHelp();

    for (;;)
    {
        printf("> ");
        readLine(line, sizeof(line));
        format(line);

        if (line[0] == '\0') continue;
        dispatchCommand(line);
    }
}

static void reportTask(void *pvParameters)
{
    ControlState state;

    vTaskDelay(pdMS_TO_TICKS(REPORT_START_OFFSET_MS));
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        if (controlStateRead(xStateMailbox, &state))
        {
            if (isPlotEnabled) printPlotLine(&state);
            else               printStatusLine(&state);
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(REPORT_PERIOD_MS));
    }
}

// Warning LED: steady while the sensor is untrusted, blinking while the
// measurement sits too far from the set point, off otherwise.
static void updateAlarmLed(const ControlState *state)
{
    if (state->sensorStatus != DHT_READ_OK)
    {
        alarmLed.turnOn();
        return;
    }

    int16_t deviation = (int16_t)(state->measuredDeciC - state->setPointDeciC);
    if (isDeviationCritical(deviation, DEVIATION_ALARM_DECI_C)) alarmLed.toggle();
    else                                                        alarmLed.turnOff();
}


static void alarmTask(void *pvParameters)
{
    ControlState state;

    alarmLed.begin();
    vTaskDelay(pdMS_TO_TICKS(ALARM_START_OFFSET_MS));
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        if (controlStateRead(xStateMailbox, &state)) updateAlarmLed(&state);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(ALARM_PERIOD_MS));
    }
}

void setup()
{
    rtosUartStdioInit(UART_BAUD, tskIDLE_PRIORITY + 1, UART_QUEUE_LEN);

    xControlQueue = xQueueCreate(CONTROL_QUEUE_LEN, sizeof(ControlCommand));
    xStateMailbox = xQueueCreate(1, sizeof(ControlState));

    xTaskCreate(cmdTask,     "CMD",     320, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(sensorTask,  "SENSOR",  192, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(controlTask, "CONTROL", 320, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(reportTask,  "REPORT",  256, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(alarmTask,   "ALARM",   128, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();
}

void loop() { }