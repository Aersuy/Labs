#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "speedScale.h"
#include "encoderSensor.h"

// The WYGH206 slot sensor has to sit on an interrupt-capable pin. On the Mega
// those are 2, 3, 18, 19, 20 and 21; pin 6 - where the schematic first put it
// - has no external interrupt and no pin-change interrupt either, since those
// cover ports B, J and K only. Polling a ~100 Hz pulse train from a task on a
// 16 ms tick would alias, so the sensor moves to 2 (INT0).
constexpr int ENCODER_PIN = 2;
constexpr int MOTOR_IN1_PIN = 8;        // same wiring as lab 4.2
constexpr int MOTOR_IN2_PIN = 7;
constexpr int MOTOR_ENABLE_PIN = 9;     // PWM, Timer2 on the Mega
constexpr int FAULT_LED_PIN = 13;

// feilipu/FreeRTOS drives the AVR tick from the watchdog, so one tick is
// portTICK_PERIOD_MS (16 ms - Atmel's "WDTO_15MS" is really 16.384 ms) and
// every delay is quantised to it. The PID divides by its sample period, so
// that period must be the one the task really achieves: it is declared in
// ticks and the millisecond value is derived from it, never the other way
// round.
//
// 6 ticks = 96 ms, about 10 Hz. The position loop ran at 2 ticks, but 20
// pulses per revolution cannot feed a 31 Hz loop: at 100 RPM a 32 ms window
// sees one pulse, sometimes none. The sample period is set by what the sensor
// can resolve, not by what the controller would like.
constexpr TickType_t CONTROL_PERIOD_TICKS = 6;
constexpr TickType_t REPORT_PERIOD_TICKS = 6;       // ~96 ms, 10 Hz for the plotter
constexpr TickType_t FAULT_PERIOD_TICKS = 13;       // ~208 ms
constexpr uint16_t CONTROL_PERIOD_MS = CONTROL_PERIOD_TICKS * portTICK_PERIOD_MS;
constexpr uint8_t STATUS_DECIMATION = 5;            // status lines every ~480 ms

constexpr int16_t DEFAULT_SET_POINT_DECI_RPM = 1200;    // 120.0 RPM
constexpr int16_t DEAD_BAND_DECI_RPM = 50;              // 5.0 RPM

constexpr uint8_t CMD_BUF_LEN = 32;
constexpr uint8_t CONTROL_QUEUE_LEN = 8;
constexpr TickType_t QUEUE_SEND_TIMEOUT_TICKS = 3;

// 9600 cannot carry 11 plot lines per second. The queue holds a whole status
// line, so the control task never blocks on the UART mid-loop - that would
// stretch the sample period the controller assumes is fixed.
constexpr unsigned long UART_BAUD = 115200;
constexpr UBaseType_t UART_QUEUE_LEN = 128;

constexpr PidConfig PID_CONFIG = {
    SPEED_MIN_DECI_RPM,
    SPEED_MAX_DECI_RPM,
    DEAD_BAND_DECI_RPM,
    CONTROL_PERIOD_MS
};

// Start P-only: the lab is tuned live from the console, term by term. 0.500
// saturates the output at 200 RPM of error, which is about full scale.
//
// Unlike the position rig, P alone cannot null the error here: a speed plant
// has no free integrator, so the shaft settles wherever the proportional term
// happens to balance friction. The droop is the point of the exercise - it is
// what ki is for. Leave kd at 0: differentiating a pulse-rate measurement
// amplifies exactly the quantisation the edge timing was chosen to avoid.
constexpr PidGains DEFAULT_GAINS = { 500, 0, 0 };

// Guards the int32_t headroom in PidController::accumulateIntegral(), which
// evaluates kiMilli * error * samplePeriodMs. Lengthening the control period
// or raising the gain ceiling must not silently overflow it. The move from a
// 32 ms loop to a 96 ms one is what brought GAIN_MILLI_MAX down to 8000.
static_assert((int64_t)PidController::GAIN_MILLI_MAX *
                  SPEED_MAX_DECI_RPM * CONTROL_PERIOD_MS < 2147483647L,
              "control period too long for the gain ceiling");

// motorOutput is pure and cannot include the Arduino-dependent driver, so its
// full-scale percent is pinned against the driver here instead.
static_assert(MOTOR_MAX_PERCENT == MotorDc::POWER_MAX,
              "motorOutput full scale must match the MotorDc driver");

static EncoderSensor speedSensor(ENCODER_PIN);
static MotorDc motor(MOTOR_IN1_PIN, MOTOR_IN2_PIN, MOTOR_ENABLE_PIN);
static Led faultLed(FAULT_LED_PIN);
static PidController pid(PID_CONFIG, DEFAULT_GAINS, DEFAULT_SET_POINT_DECI_RPM);

static QueueHandle_t xControlQueue = NULL;
static QueueHandle_t xStateMailbox = NULL;

// atomic no queue needed
static volatile bool isPlotEnabled = false;

// deciTemperature is unit-agnostic tenths text I/O; the name is a leftover
// from lab 5.1, the maths is the same for RPM.
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
    printDeci(state->measuredDeciRpm);
    printf(" SP=");
    printDeci(state->setPointDeciRpm);
    printf(" e=");
    printDeci(state->terms.errorDeci);
    printf(" | P=%d I=%d D=%d OUT=%d | motor=%+d%% | sat=%d db=%d senzor=%s n=%u\n",
           state->terms.proportionalPerMille,
           state->terms.integralPerMille,
           state->terms.derivativePerMille,
           state->terms.outputPerMille,
           state->motorPercent,
           state->terms.isSaturated ? 1 : 0,
           state->terms.isInDeadBand ? 1 : 0,
           speedReadStatusName(state->sensorStatus),
           state->sampleCount);
}

// Arduino Serial Plotter reads "label:value" pairs and auto-scales each trace.
static void printPlotLine(const PidControlState *state)
{
    printf("SP:%d PV:%d OUT:%d\n",
           state->setPointDeciRpm,
           state->measuredDeciRpm,
           state->terms.outputPerMille);
}

static void printHelp()
{
    printf("Comenzi:\n");
    printf("  sp <rpm>     - set point, ex. sp 120.0\n");
    printf("  kp <val>     - castig proportional, ex. kp 0.500\n");
    printf("  ki <val>     - castig integral, ex. ki 0.200\n");
    printf("  kd <val>     - castig derivativ, ex. kd 0.000\n");
    printf("  duty <proc>  - bucla deschisa, 0..100 %%, pentru calibrare\n");
    printf("  reset        - sterge integratorul si memoria derivativa\n");
    printf("  stop | run   - dezactiveaza / activeaza bucla\n");
    printf("  plot on|off  - linii pentru Arduino Serial Plotter\n");
    printf("  status       - raport imediat\n");
    printf("  help         - lista de comenzi\n");
}

static PidControlState buildInitialState()
{
    PidControlState state;

    state.setPointDeciRpm = pid.getSetPointDeci();
    state.measuredDeciRpm = 0;
    state.terms = pid.getTerms();
    state.gains = pid.getGains();
    state.motorPercent = 0;
    state.sensorStatus = SPEED_READ_OK;
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

// A single-channel encoder cannot see direction, so the controller has no way
// to verify a reversal: a backwards command would be flying blind. It is also
// not what a negative output means for a motor - "slower" is achieved by
// easing off, not by driving the other way, and the shaft coasts down on its
// own. So the drive stays one-way and the output floor is zero.
static int16_t forwardOnly(int16_t outputPerMille)
{
    return (outputPerMille < 0) ? 0 : outputPerMille;
}

static void applyMeasurement(const SpeedSample *sample, PidControlState *state)
{
    state->measuredDeciRpm = sample->speedDeciRpm;
    state->sensorStatus = sample->status;
    state->sampleCount++;

    if (!state->isLoopEnabled)
    {
        return;
    }

    int16_t output = pid.update(sample->speedDeciRpm);
    state->terms = pid.getTerms();
    driveMotor(perMilleToMotorPercent(forwardOnly(output)), state);
}

static void applySetPoint(int16_t setPointDeciRpm, PidControlState *state)
{
    if (!pid.setSetPoint(setPointDeciRpm))
    {
        printf("[err] set point in afara domeniului (");
        printDeci(SPEED_MIN_DECI_RPM);
        printf(" .. ");
        printDeci(SPEED_MAX_DECI_RPM);
        printf(")\n");
        return;
    }
    state->setPointDeciRpm = pid.getSetPointDeci();
    printf("[pid] set point nou: ");
    printDeci(state->setPointDeciRpm);
    printf(" rpm\n");
}

static void applyGainChange(bool isAccepted, PidControlState *state)
{
    if (!isAccepted)
    {
        printf("[err] castig in afara domeniului (0.000 .. 8.000)\n");
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

// Resetting on re-enable is what stops a stale integral from spinning the
// shaft up the moment the loop comes back.
static void applyLoopEnable(bool isEnabled, PidControlState *state)
{
    state->isLoopEnabled = isEnabled;
    pid.reset();
    state->terms = pid.getTerms();
    holdMotorStopped(state);
    printf("[pid] bucla %s\n", isEnabled ? "activata" : "oprita");
}

// Open loop on purpose: the two constants the closed loop is built on -
// SPEED_MAX_DECI_RPM and MOTOR_MIN_MOVE_PERCENT - cannot be derived, only
// measured, and measuring them means commanding a duty and reading the
// resulting speed with no controller in the way.
static void applyDuty(int16_t percent, PidControlState *state)
{
    state->isLoopEnabled = false;
    pid.reset();
    state->terms = pid.getTerms();

    driveMotor((int)percent, state);
    printf("[motor] bucla deschisa, duty %d%% (scrie run pentru PID)\n", percent);
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
        case PID_CMD_DUTY:
            applyDuty(command->payload.value, state);
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

// Collapses runs of whitespace so "sp   120.0" parses like "sp 120.0".
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

// atoi() would read "12abc" as 12 and an empty string as 0; the duty command
// is the one place a whole integer enters the system, so it is validated here
// rather than trusted.
static bool parseWholeNumber(const char *text, int16_t minimum, int16_t maximum,
                             int16_t *out)
{
    if (*text == '\0') return false;

    int32_t value = 0;
    for (const char *c = text; *c != '\0'; c++)
    {
        if (!isdigit((unsigned char)*c)) return false;

        value = value * 10 + (*c - '0');
        if (value > maximum) return false;
    }
    if (value < minimum) return false;

    *out = (int16_t)value;
    return true;
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

static bool handleDutyCommand(const char *line)
{
    if (!hasCommandPrefix(line, "duty")) return false;

    const char *argument = line + strlen("duty") + 1;
    int16_t percent = 0;
    if (!parseWholeNumber(argument, 0, MOTOR_MAX_PERCENT, &percent))
    {
        printf("[err] duty invalid: \"%s\" (0..%d)\n", argument, MOTOR_MAX_PERCENT);
        return true;
    }
    submitCommand(PID_CMD_DUTY, percent);
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
    if (handleDutyCommand(line)) return;
    if (handlePlotCommand(line)) return;
    if (handleKeywordCommand(line)) return;

    printf("[err] comanda necunoscuta: \"%s\" (scrie help)\n", line);
}

// Owns the encoder: the only task that reads the pulse counters. Its fixed
// cadence is what lets the PID treat dt as a constant - the speed itself is
// timed from the pulse edges, not from this wake-up, so a late tick costs a
// sample but never corrupts the measurement.
static void sensorTask(void *pvParameters)
{
    PidCommand command;
    command.type = PID_CMD_MEASUREMENT;

    speedSensor.begin();
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        EncoderDelta delta = speedSensor.read();
        command.payload.sample = buildSpeedSample(&delta);
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

    printf("\n=== Lab 5.2: control PID de turatie (encoder WYGH206 + L293D) ===\n");
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

// Steady while the shaft is commanded to move but no pulses arrive - a
// blocked gearbox, a dead bridge or an unplugged sensor, which is the one
// fault this rig can actually detect - blinking while the controller is
// saturated, off otherwise. A stall with the motor stopped is just a stopped
// motor, not a fault.
static void updateFaultLed(const PidControlState *state)
{
    bool isDriven = state->motorPercent >= MOTOR_MIN_MOVE_PERCENT;

    if (state->sensorStatus == SPEED_READ_STALLED && isDriven)
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
