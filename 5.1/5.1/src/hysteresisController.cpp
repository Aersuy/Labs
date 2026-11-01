#include "hysteresisController.h"

static const int16_t HALF_BAND_DIVISOR = 2;

const char *controlOutputName(ControlOutput output)
{
    return (output == CONTROL_OUTPUT_ON) ? "ON" : "OFF";
}

const char *controlModeName(ControlMode mode)
{
    return (mode == CONTROL_MODE_HEATING) ? "HEAT" : "COOL";
}

bool isDeviationCritical(int16_t deviationDeciC, int16_t thresholdDeciC)
{
    int16_t magnitude = (deviationDeciC < 0) ? (int16_t)(-deviationDeciC)
                                             : deviationDeciC;
    return magnitude > thresholdDeciC;
}
HysteresisController::HysteresisController(int16_t setPointDeciC,
                                           int16_t hysteresisDeciC,
                                           ControlMode mode)
{
    this->setPointDeciC = setPointDeciC;
    this->hysteresisDeciC = hysteresisDeciC;
    this->mode = mode;
    this->output = CONTROL_OUTPUT_OFF;
}
bool HysteresisController::setSetPoint(int16_t setPointDeciC)
{
    if (setPointDeciC < HysteresisController::SET_POINT_MIN_DECI_C) return false;
    if (setPointDeciC > HysteresisController::SET_POINT_MAX_DECI_C) return false;

    this->setPointDeciC = setPointDeciC;
    return true;
}

bool HysteresisController::setHysteresis(int16_t hysteresisDeciC)
{
    if (hysteresisDeciC < HysteresisController::HYSTERESIS_MIN_DECI_C) return false;
    if (hysteresisDeciC > HysteresisController::HYSTERESIS_MAX_DECI_C) return false;

    this->hysteresisDeciC = hysteresisDeciC;
    return true;
}

void HysteresisController::setMode(ControlMode mode)
{
    this->mode = mode;
}

int16_t HysteresisController::lowerThresholdDeciC() const
{
    return (int16_t)(this->setPointDeciC - this->hysteresisDeciC / HALF_BAND_DIVISOR);
}

int16_t HysteresisController::upperThresholdDeciC() const
{
    return (int16_t)(this->setPointDeciC + this->hysteresisDeciC / HALF_BAND_DIVISOR);
}

int16_t HysteresisController::deviationDeciC(int16_t measuredDeciC) const
{
    return (int16_t)(measuredDeciC - this->setPointDeciC);
}

// Heater: cold enough -> ON, warm enough -> OFF, in between -> unchanged.
ControlOutput HysteresisController::decideHeating(int16_t measuredDeciC) const
{
    if (measuredDeciC < this->lowerThresholdDeciC()) return CONTROL_OUTPUT_ON;
    if (measuredDeciC > this->upperThresholdDeciC()) return CONTROL_OUTPUT_OFF;
    return this->output;
}

ControlOutput HysteresisController::decideCooling(int16_t measuredDeciC) const
{
    if (measuredDeciC > this->upperThresholdDeciC()) return CONTROL_OUTPUT_ON;
    if (measuredDeciC < this->lowerThresholdDeciC()) return CONTROL_OUTPUT_OFF;
    return this->output;
}

ControlOutput HysteresisController::update(int16_t measuredDeciC)
{
    this->output = (this->mode == CONTROL_MODE_HEATING)
                   ? this->decideHeating(measuredDeciC)
                   : this->decideCooling(measuredDeciC);
    return this->output;
}

ControlOutput HysteresisController::getOutput() const
{
    return this->output;
}

int16_t HysteresisController::getSetPointDeciC() const
{
    return this->setPointDeciC;
}

int16_t HysteresisController::getHysteresisDeciC() const
{
    return this->hysteresisDeciC;
}

ControlMode HysteresisController::getMode() const
{
    return this->mode;
}