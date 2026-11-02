#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "uartStdio.h"
#include "ledClass.h"
#include "motorDc.h"
#include "deciTemperature.h"
#include "milliGain.h"
#include "motorOutput.h"
#include "pidController.h"
#include "pidControlState.h"
#include "positionScale.h"
#include "potPositionSensor.h"

constexpr int POSITION_POT_PIN = A0;
constexpr int MOTOR_IN1_PIN = 8;        // same wiring as lab 4.2
constexpr int MOTOR_IN2_PIN = 7;
constexpr int MOTOR_ENABLE_PIN = 9;     // PWM, Timer2 on the Mega
constexpr int FAULT_LED_PIN = 13;

// feilipu/FreeRTOS drives the AVR tick from the watchdog, so one tick is
// portTICK_PERIOD_MS (16 ms - Atmel's "WDTO_15MS" is really 16.384 ms) and
// every delay is quantised to it. The PID divides by its sample period, so
// that period must be the one the task really achieves: it is declared in
// ticks and the millisecond value is derived from it, never the other way
// round. 2 ticks = 32 ms, about 31 Hz. The 2% between the nominal 16 ms and
// the true 16.384 ms is absorbed into the hand-tuned gains.
constexpr TickType_t CONTROL_PERIOD_TICKS = 2;
constexpr TickType_t REPORT_PERIOD_TICKS = 6;       // ~96 ms, 10 Hz for the plotter
constexpr TickType_t FAULT_PERIOD_TICKS = 13;       // ~208 ms
constexpr uint16_t CONTROL_PERIOD_MS = CONTROL_PERIOD_TICKS * portTICK_PERIOD_MS;
constexpr uint8_t STATUS_DECIMATION = 5;            // status lines every ~480 ms

constexpr int16_t DEFAULT_SET_POINT_DECI_DEG = 1350;    // 135.0 deg, mid travel
constexpr int16_t DEAD_BAND_DECI_DEG = 20;              // 2.0 deg

constexpr uint8_t CMD_BUF_LEN = 32;
constexpr uint8_t CONTROL_QUEUE_LEN = 8;
constexpr TickType_t QUEUE_SEND_TIMEOUT_TICKS = 3;

// 9600 cannot carry 11 plot lines per second. The queue holds a whole status
// line, so the control task never blocks on the UART mid-loop - that would
// stretch the sample period the controller assumes is fixed.
constexpr unsigned long UART_BAUD = 115200;
constexpr UBaseType_t UART_QUEUE_LEN = 128;

constexpr PidConfig PID_CONFIG = {
    POSITION_MIN_DECI_DEG,
    POSITION_MAX_DECI_DEG,
    DEAD_BAND_DECI_DEG,
    CONTROL_PERIOD_MS
};

// Start P-only: the lab is tuned live from the console, term by term.
constexpr PidGains DEFAULT_GAINS = { 5000, 0, 0 };

// Guards the int32_t headroom in PidController::accumulateIntegral(), which
// evaluates kiMilli * error * samplePeriodMs. Lengthening the control period
// or raising the gain ceiling must not silently overflow it.
static_assert((int64_t)PidController::GAIN_MILLI_MAX *
                  POSITION_MAX_DECI_DEG * CONTROL_PERIOD_MS < 2147483647L,
              "control period too long for the gain ceiling");

// motorOutput is pure and cannot include the Arduino-dependent driver, so its
// full-scale percent is pinned against the driver here instead.
static_assert(MOTOR_MAX_PERCENT == MotorDc::POWER_MAX,
              "motorOutput full scale must match the MotorDc driver");

static PotPositionSensor positionSensor(POSITION_POT_PIN);
static MotorDc motor(MOTOR_IN1_PIN, MOTOR_IN2_PIN, MOTOR_ENABLE_PIN);
static Led faultLed(FAULT_LED_PIN);
static PidController pid(PID_CONFIG, DEFAULT_GAINS, DEFAULT_SET_POINT_DECI_DEG);

static QueueHandle_t xControlQueue = NULL;
static QueueHandle_t xStateMailbox = NULL;

// atomic no queue needed
static volatile bool isPlotEnabled = false;

// deciTemperature is unit-agnostic tenths text I/O; the name is a leftover
// from lab 5.1, the maths is the same for degrees of rotation.
static void printDeci(int16_t value)
{
    char text[DECI_TEXT_MIN_LEN];
    formatDeciCelsius(value, text, sizeof(text));
    printf("%s", text);
}

static void printMilli(int16_t value)
{
    char text[MILLI_TEXT_MIN_LEN];
    formatMilliGain(value, text, sizeof(text));
    printf("%s", text);
}

static void printGains(const PidGains *gains)
{
    printf("[pid] kp=");
    printMilli(gains->kpMilli);
    printf(" ki=");
    printMilli(gains->kiMilli);
    printf(" kd=");
    printMilli(gains->kdMilli);
    printf("\n");
}

static void printStatusLine(const PidControlState *state)
{
    printf("[status] PV=");
    printDeci(state->measuredDeciDeg);
    printf(" SP=");
    printDeci(state->setPointDeciDeg);
    printf(" e=");
    printDeci(state->terms.errorDeciDeg);
    printf(" | P=%d I=%d D=%d OUT=%d | motor=%+d%% | sat=%d db=%d senzor=%s n=%u\n",
           state->terms.proportionalPerMille,
           state->terms.integralPerMille,
           state->terms.derivativePerMille,
           state->terms.outputPerMille,
           state->motorPercent,
           state->terms.isSaturated ? 1 : 0,
           state->terms.isInDeadBand ? 1 : 0,
           positionReadStatusName(state->sensorStatus),
           state->sampleCount);
}

// Arduino Serial Plotter reads "label:value" pairs and auto-scales each trace.
static void printPlotLine(const PidControlState *state)
{
    printf("SP:%d PV:%d OUT:%d\n",
           state->setPointDeciDeg,
           state->measuredDeciDeg,
           state->terms.outputPerMille);
}

static void printHelp()
{
    printf("Comenzi:\n");
    printf("  sp <grade>   - set point, ex. sp 135.0\n");
    printf("  kp <val>     - castig proportional, ex. kp 5.0\n");
    printf("  ki <val>     - castig integral, ex. ki 0.050\n");
    printf("  kd <val>     - castig derivativ, ex. kd 0.200\n");
    printf("  reset        - sterge integratorul si memoria derivativa\n");
    printf("  stop | run   - dezactiveaza / activeaza bucla\n");
    printf("  plot on|off  - linii pentru Arduino Serial Plotter\n");
    printf("  status       - raport imediat\n");
    printf("  help         - lista de comenzi\n");
}

static PidControlState buildInitialState()
{
    PidControlState state;

    state.setPointDeciDeg = pid.getSetPointDeciDeg();
    state.measuredDeciDeg = 0;
    state.terms = pid.getTerms();
    state.gains = pid.getGains();
    state.motorPercent = 0;
    state.sensorStatus = POSITION_READ_OK;
    state.isLoopEnabled = true;
    state.sampleCount = 0;
    return state;
}

// perMilleToMotorPercent is clamped to +/-100, which MotorDc always accepts;
// the rejection path is still reported rather than swallowed.
static void driveMotor(int percent, PidControlState *state)
{
    state->motorPercent = percent;
    if (!motor.setPower(percent))
    {
        printf("[motor] comanda respinsa: %d%%\n", percent);
    }
}

static void holdMotorStopped(PidControlState *state)
{
    motor.stop();
    state->motorPercent = 0;
}

static void applyMeasurement(const PositionSample *sample, PidControlState *state)
{
    state->measuredDeciDeg = sample->positionDeciDeg;
    state->sensorStatus = sample->status;
    state->sampleCount++;

    if (!state->isLoopEnabled)
    {
        holdMotorStopped(state);
        return;
    }

    int16_t output = pid.update(sample->positionDeciDeg);
    state->terms = pid.getTerms();
    driveMotor(perMilleToMotorPercent(output), state);
}

static void applySetPoint(int16_t setPointDeciDeg, PidControlState *state)
{
    if (!pid.setSetPoint(setPointDeciDeg))
    {
        printf("[err] set point in afara domeniului (");
        printDeci(POSITION_MIN_DECI_DEG);
        printf(" .. ");
        printDeci(POSITION_MAX_DECI_DEG);
        printf(")\n");
        return;
    }
    state->setPointDeciDeg = pid.getSetPointDeciDeg();
    printf("[pid] set point nou: ");
    printDeci(state->setPointDeciDeg);
    printf(" grade\n");
}

static void applyGainChange(bool isAccepted, PidControlState *state)
{
    if (!isAccepted)
    {
        printf("[err] castig in afara domeniului (0.000 .. 20.000)\n");
        return;
    }
    state->gains = pid.getGains();
    printGains(&state->gains);
}

static void applyReset(PidControlState *state)
{
    pid.reset();
    state->terms = pid.getTerms();
    holdMotorStopped(state);
    printf("[pid] integrator si memorie derivativa sterse\n");
}

// Resetting on re-enable is what stops a stale integral from kicking the shaft
// the moment the loop comes back.
static void applyLoopEnable(bool isEnabled, PidControlState *state)
{
    state->isLoopEnabled = isEnabled;
    pid.reset();
    state->terms = pid.getTerms();
    holdMotorStopped(state);
    printf("[pid] bucla %s\n", isEnabled ? "activata" : "oprita");
}

static void applyStatusRequest(const PidControlState *state)
{
    printStatusLine(state);
    printGains(&state->gains);
}

static void applyPidCommand(const PidCommand *command, PidControlState *state)
{
    switch (command->type)
    {
        case PID_CMD_MEASUREMENT:
            applyMeasurement(&command->payload.sample, state);
            break;
        case PID_CMD_SET_POINT:
            applySetPoint(command->payload.value, state);
            break;
        case PID_CMD_KP:
            applyGainChange(pid.setKp(command->payload.value), state);
            break;
        case PID_CMD_KI:
            applyGainChange(pid.setKi(command->payload.value), state);
            break;
        case PID_CMD_KD:
            applyGainChange(pid.setKd(command->payload.value), state);
            break;
        case PID_CMD_RESET:
            applyReset(state);
            break;
        case PID_CMD_LOOP_ENABLE:
            applyLoopEnable(command->payload.value != 0, state);
            break;
        case PID_CMD_STATUS:
            applyStatusRequest(state);
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

// Collapses runs of whitespace so "sp   135.0" parses like "sp 135.0".
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

static void submitCommand(PidCommandType type, int16_t value)
{
    PidCommand command;
    command.type = type;
    command.payload.value = value;

    if (xQueueSend(xControlQueue, &command, QUEUE_SEND_TIMEOUT_TICKS) != pdPASS)
    {
        printf("[err] coada de comenzi plina, comanda ignorata\n");
    }
}

// Numeric arguments are validated right where they enter the system, so the
// control task only ever sees well-formed fixed-point values.
static bool handleSetPointCommand(const char *line)
{
    if (!hasCommandPrefix(line, "sp")) return false;

    const char *argument = line + strlen("sp") + 1;
    int16_t value = 0;
    if (!parseDeciCelsius(argument, &value))
    {
        printf("[err] valoare invalida: \"%s\"\n", argument);
        return true;
    }
    submitCommand(PID_CMD_SET_POINT, value);
    return true;
}

static bool handleGainCommand(const char *line, const char *prefix,
                              PidCommandType type)
{
    if (!hasCommandPrefix(line, prefix)) return false;

    const char *argument = line + strlen(prefix) + 1;
    int16_t value = 0;
    if (!parseMilliGain(argument, &value))
    {
        printf("[err] castig invalid: \"%s\"\n", argument);
        return true;
    }
    submitCommand(type, value);
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

static bool handleKeywordCommand(const char *line)
{
    if (!strcmp(line, "status"))
    {
        submitCommand(PID_CMD_STATUS, 0);
        return true;
    }
    if (!strcmp(line, "reset"))
    {
        submitCommand(PID_CMD_RESET, 0);
        return true;
    }
    if (!strcmp(line, "stop"))
    {
        submitCommand(PID_CMD_LOOP_ENABLE, 0);
        return true;
    }
    if (!strcmp(line, "run"))
    {
        submitCommand(PID_CMD_LOOP_ENABLE, 1);
        return true;
    }
    if (!strcmp(line, "help"))
    {
        printHelp();
        return true;
    }
    return false;
}

static void dispatchCommand(const char *line)
{
    if (handleSetPointCommand(line)) return;
    if (handleGainCommand(line, "kp", PID_CMD_KP)) return;
    if (handleGainCommand(line, "ki", PID_CMD_KI)) return;
    if (handleGainCommand(line, "kd", PID_CMD_KD)) return;
    if (handlePlotCommand(line)) return;
    if (handleKeywordCommand(line)) return;

    printf("[err] comanda necunoscuta: \"%s\" (scrie help)\n", line);
}

// Owns the potentiometer: the only task that reads the ADC. Its fixed cadence
// is what lets the PID treat dt as a constant.
static void sensorTask(void *pvParameters)
{
    PidCommand command;
    command.type = PID_CMD_MEASUREMENT;

    positionSensor.begin();
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        command.payload.sample = positionSensor.read();
        xQueueSend(xControlQueue, &command, QUEUE_SEND_TIMEOUT_TICKS);

        vTaskDelayUntil(&lastWake, CONTROL_PERIOD_TICKS);
    }
}

// Owns the motor and the controller: no other task touches either.
static void controlTask(void *pvParameters)
{
    PidCommand command;
    PidControlState state = buildInitialState();

    motor.begin();
    pidControlStatePublish(xStateMailbox, &state);

    for (;;)
    {
        if (xQueueReceive(xControlQueue, &command, portMAX_DELAY) != pdPASS) continue;

        applyPidCommand(&command, &state);
        pidControlStatePublish(xStateMailbox, &state);
    }
}

static void cmdTask(void *pvParameters)
{
    char line[CMD_BUF_LEN];

    printf("\n=== Lab 5.2: control PID de pozitie (potentiometru + L293D) ===\n");
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

static void reportOnce(const PidControlState *state, uint8_t *ticksSinceStatus)
{
    if (isPlotEnabled)
    {
        printPlotLine(state);
        return;
    }

    (*ticksSinceStatus)++;
    if (*ticksSinceStatus < STATUS_DECIMATION) return;

    *ticksSinceStatus = 0;
    printStatusLine(state);
}

static void reportTask(void *pvParameters)
{
    PidControlState state;
    uint8_t ticksSinceStatus = 0;

    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        if (pidControlStateRead(xStateMailbox, &state))
        {
            reportOnce(&state, &ticksSinceStatus);
        }
        vTaskDelayUntil(&lastWake, REPORT_PERIOD_TICKS);
    }
}

// Steady while the wiper sits on a rail (end of travel or a broken pot),
// blinking while the controller is saturated, off otherwise.
static void updateFaultLed(const PidControlState *state)
{
    if (state->sensorStatus != POSITION_READ_OK)
    {
        faultLed.turnOn();
        return;
    }
    if (state->terms.isSaturated) faultLed.toggle();
    else                          faultLed.turnOff();
}

static void faultTask(void *pvParameters)
{
    PidControlState state;

    faultLed.begin();
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        if (pidControlStateRead(xStateMailbox, &state)) updateFaultLed(&state);

        vTaskDelayUntil(&lastWake, FAULT_PERIOD_TICKS);
    }
}

void setup()
{
    rtosUartStdioInit(UART_BAUD, tskIDLE_PRIORITY + 1, UART_QUEUE_LEN);

    xControlQueue = xQueueCreate(CONTROL_QUEUE_LEN, sizeof(PidCommand));
    xStateMailbox = xQueueCreate(1, sizeof(PidControlState));

    xTaskCreate(cmdTask,     "CMD",     320, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(sensorTask,  "SENSOR",  192, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(controlTask, "CONTROL", 320, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(reportTask,  "REPORT",  256, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(faultTask,   "FAULT",   128, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();
}

void loop() { }