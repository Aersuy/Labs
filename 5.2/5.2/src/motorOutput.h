#ifndef MOTOROUTPUT_H
#define MOTOROUTPUT_H

#include <stdint.h>

#include "perMille.h"

// A geared DC motor does not turn below roughly a quarter of full duty; under
// that the bridge only makes it hum. This is a property of the actuator, not
// of the control law, which is why it lives here and not in PidController.
const int MOTOR_MIN_MOVE_PERCENT = 25;
const int MOTOR_MAX_PERCENT = 100;

// 0 stays 0; any other value is mapped linearly onto
// [MOTOR_MIN_MOVE_PERCENT .. MOTOR_MAX_PERCENT] with the sign preserved.
// The result is always within +/-100, so MotorDc::setPower never rejects it.
int perMilleToMotorPercent(int16_t outputPerMille);

#endif