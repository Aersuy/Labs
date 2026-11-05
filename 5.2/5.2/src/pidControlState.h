#ifndef PIDCONTROLSTATE_H
#define PIDCONTROLSTATE_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <queue.h>

#include "pidController.h"
#include "speedScale.h"

// Snapshot published by the control task and consumed by the report and fault
// tasks through a one-slot mailbox.
typedef struct {
    int16_t setPointDeciRpm;
    int16_t measuredDeciRpm;
    PidTerms terms;
    PidGains gains;
    int motorPercent;
    SpeedReadStatus sensorStatus;
    bool isLoopEnabled;
    uint16_t sampleCount;
} PidControlState;

typedef enum {
    PID_CMD_MEASUREMENT = 0,
    PID_CMD_SET_POINT,
    PID_CMD_KP,
    PID_CMD_KI,
    PID_CMD_KD,
    PID_CMD_RESET,
    PID_CMD_LOOP_ENABLE,
    PID_CMD_STATUS,
    // Open-loop duty, for characterising the motor before any gain is tuned:
    // it is the only way to find the real full-scale speed and the real
    // stiction floor, and both are constants the closed loop depends on.
    PID_CMD_DUTY
} PidCommandType;

// Tagged union: the sensor task and the command task feed the same queue, so
// exactly one task ever touches the motor and the controller.
typedef struct {
    PidCommandType type;
    union {
        int16_t value;       // set point in tenths, gains in thousandths,
                             // LOOP_ENABLE as 0/1, DUTY in percent
        SpeedSample sample;  // MEASUREMENT
    } payload;
} PidCommand;

void pidControlStatePublish(QueueHandle_t mailbox, const PidControlState *state);
bool pidControlStateRead(QueueHandle_t mailbox, PidControlState *out);

#endif
