#ifndef CONTROLSTATE_H
#define CONTROLSTATE_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <queue.h>

#include "dhtSensor.h"
#include "hysteresisController.h"

// Snapshot published by the control task and consumed by the report and alarm
// tasks through a one-slot mailbox.
typedef struct {
    int16_t measuredDeciC;
    int16_t humidityDeciPercent;
    int16_t setPointDeciC;
    int16_t hysteresisDeciC;
    int16_t lowerThresholdDeciC;
    int16_t upperThresholdDeciC;
    ControlOutput output;
    ControlMode mode;
    DhtReadStatus sensorStatus;
    uint16_t switchCount;
} ControlState;

typedef enum {
    CONTROL_CMD_MEASUREMENT = 0,
    CONTROL_CMD_SET_POINT,
    CONTROL_CMD_HYSTERESIS,
    CONTROL_CMD_MODE,
    CONTROL_CMD_STATUS
} ControlCommandType;

// Tagged union: the sensor task and the command task feed the same queue, so
// exactly one task ever touches the relay and the controller.
typedef struct {
    ControlCommandType type;
    union {
        int16_t value;      // SET_POINT / HYSTERESIS in tenths, MODE as ControlMode
        DhtSample sample;   // MEASUREMENT
    } payload;
} ControlCommand;

void controlStatePublish(QueueHandle_t mailbox, const ControlState *state);
bool controlStateRead(QueueHandle_t mailbox, ControlState *out);

#endif