#ifndef HYSTERESISCONTROLLER_H
#define HYSTERESISCONTROLLER_H

#include <stdint.h>

typedef enum {
    CONTROL_OUTPUT_OFF = 0,
    CONTROL_OUTPUT_ON = 1
} ControlOutput;

typedef enum {
    CONTROL_MODE_HEATING = 0,
    CONTROL_MODE_COOLING = 1
} ControlMode;

const char *controlOutputName(ControlOutput output);
const char *controlModeName(ControlMode mode);

// True when the measurement sits further from the set point than the alarm
// threshold allows, in either direction.
bool isDeviationCritical(int16_t deviationDeciC, int16_t thresholdDeciC);

// ON-OFF law with a dead band centred on the set point. Heating turns the
// actuator ON below the lower threshold and OFF above the upper one; inside
// the band the previous output is kept, and that memory is what suppresses
// chattering. Deliberately free of I/O and of Arduino.h: the task that owns
// the relay applies the returned decision, so this class stays deterministic
// and testable on the host.
class HysteresisController {
    public:
    HysteresisController(int16_t setPointDeciC,
                         int16_t hysteresisDeciC,
                         ControlMode mode);

    bool setSetPoint(int16_t setPointDeciC);        // false if out of range
    bool setHysteresis(int16_t hysteresisDeciC);    // false if out of range
    void setMode(ControlMode mode);

    ControlOutput update(int16_t measuredDeciC);

    ControlOutput getOutput() const;
    int16_t getSetPointDeciC() const;
    int16_t getHysteresisDeciC() const;
    ControlMode getMode() const;

    int16_t lowerThresholdDeciC() const;
    int16_t upperThresholdDeciC() const;
    int16_t deviationDeciC(int16_t measuredDeciC) const;

    // Working range of the thermistor channel: 0..50 C.
    static const int16_t SET_POINT_MIN_DECI_C = 0;
    static const int16_t SET_POINT_MAX_DECI_C = 500;
    // A band narrower than 1.0 C would sit inside the noise of the ADC
    // reading and make the actuator switch on nearly every sample.
    static const int16_t HYSTERESIS_MIN_DECI_C = 10;
    static const int16_t HYSTERESIS_MAX_DECI_C = 100;

    private:
    ControlOutput decideHeating(int16_t measuredDeciC) const;
    ControlOutput decideCooling(int16_t measuredDeciC) const;

    int16_t setPointDeciC;
    int16_t hysteresisDeciC;
    ControlMode mode;
    ControlOutput output;
};

#endif