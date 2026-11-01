#ifndef PIDCONTROLSTATE_H
#define PIDCONTROLSTATE_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <queue.h>

#include "pidController.h"
#include "positionScale.h"

// Snapshot published by the control task and consumed by the report and fault
// tasks through a one-slot mailbox.
typedef struct {
    int16_t setPointDeciDeg;
    int16_t measuredDeciDeg;
    PidTerms terms;
    PidGains gains;
    int motorPercent;
    PositionReadStatus sensorStatus;
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
    PID_CMD_STATUS
} PidCommandType;

// Tagged union: the sensor task and the command task feed the same queue, so
// exactly one task ever touches the motor and the controller.
typedef struct {
    PidCommandType type;
    union {
        int16_t value;          // set point in tenths, gains in thousandths,
                                // LOOP_ENABLE as 0/1
        PositionSample sample;  // MEASUREMENT
    } payload;
} PidCommand;

void pidControlStatePublish(QueueHandle_t mailbox, const PidControlState *state);
bool pidControlStateRead(QueueHandle_t mailbox, PidControlState *out);

#endif