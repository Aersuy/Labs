#include "pidController.h"

// Gains are stored multiplied by this, so every term divides it back out.
static const int32_t GAIN_MILLI_SCALE = 1000;

static void clearTerms(PidTerms *terms)
{
    terms->errorDeci = 0;
    terms->proportionalPerMille = 0;
    terms->integralPerMille = 0;
    terms->derivativePerMille = 0;
    terms->outputPerMille = 0;
    terms->isSaturated = false;
    terms->isInDeadBand = false;
}

static bool isGainInRange(int16_t gainMilli)
{
    return gainMilli >= PidController::GAIN_MILLI_MIN &&
           gainMilli <= PidController::GAIN_MILLI_MAX;
}

PidController::PidController(const PidConfig &config, PidGains gains,
                             int16_t setPointDeci)
{
    this->config = config;
    this->gains = gains;
    this->setPointDeci = setPointDeci;
    this->integralMilliPerMille = 0;
    this->previousMeasuredDeci = 0;
    this->hasPreviousMeasurement = false;
    clearTerms(&this->terms);
}

bool PidController::setGains(const PidGains &gains)
{
    if (!isGainInRange(gains.kpMilli)) return false;
    if (!isGainInRange(gains.kiMilli)) return false;
    if (!isGainInRange(gains.kdMilli)) return false;

    this->gains = gains;
    return true;
}

bool PidController::setKp(int16_t kpMilli)
{
    PidGains updated = this->gains;
    updated.kpMilli = kpMilli;
    return this->setGains(updated);
}

bool PidController::setKi(int16_t kiMilli)
{
    PidGains updated = this->gains;
    updated.kiMilli = kiMilli;
    return this->setGains(updated);
}

bool PidController::setKd(int16_t kdMilli)
{
    PidGains updated = this->gains;
    updated.kdMilli = kdMilli;
    return this->setGains(updated);
}

bool PidController::setSetPoint(int16_t setPointDeci)
{
    if (setPointDeci < this->config.setPointMin) return false;
    if (setPointDeci > this->config.setPointMax) return false;

    this->setPointDeci = setPointDeci;
    return true;
}

void PidController::reset()
{
    this->integralMilliPerMille = 0;
    this->previousMeasuredDeci = 0;
    this->hasPreviousMeasurement = false;
    clearTerms(&this->terms);
}

int16_t PidController::update(int16_t measuredDeci)
{
    int16_t error = (int16_t)(this->setPointDeci - measuredDeci);

    this->terms.errorDeci = error;
    this->terms.isInDeadBand = this->isWithinDeadBand(error);

    if (this->terms.isInDeadBand) this->holdInDeadBand();
    else                          this->computeTerms(error, measuredDeci);

    this->previousMeasuredDeci = measuredDeci;
    this->hasPreviousMeasurement = true;
    return this->terms.outputPerMille;
}


void PidController::computeTerms(int16_t error, int16_t measuredDeci)
{
    this->accumulateIntegral(error);

    int32_t proportional = this->proportionalTerm(error);
    int32_t integral = this->integralMilliPerMille / GAIN_MILLI_SCALE;
    int32_t derivative = this->derivativeTerm(measuredDeci);
    int32_t sum = proportional + integral + derivative;

    this->terms.proportionalPerMille = clampPerMille(proportional);
    this->terms.integralPerMille = clampPerMille(integral);
    this->terms.derivativePerMille = clampPerMille(derivative);
    this->terms.outputPerMille = clampPerMille(sum);
    this->terms.isSaturated = (sum != (int32_t)this->terms.outputPerMille);
}

// Inside the dead band the actuator is released and the history frozen. A
// geared motor cannot resolve a fraction of a degree, so without this the
// shaft hunts around the target indefinitely and the gearbox buzzes.
void PidController::holdInDeadBand()
{
    this->terms.proportionalPerMille = 0;
    this->terms.derivativePerMille = 0;
    this->terms.integralPerMille =
        clampPerMille(this->integralMilliPerMille / GAIN_MILLI_SCALE);
    this->terms.outputPerMille = 0;
    this->terms.isSaturated = false;
}

int32_t PidController::proportionalTerm(int16_t error) const
{
    return ((int32_t)this->gains.kpMilli * error) / GAIN_MILLI_SCALE;
}
// Ki is folded into the accumulator rather than applied on readout, so
// retuning ki does not rescale the stored history and the loop stays bumpless
// across a gain change. The clamp is the anti-windup: a mechanically blocked
// shaft cannot build a history the controller needs seconds to unwind.
void PidController::accumulateIntegral(int16_t error)
{
    int32_t increment = ((int32_t)this->gains.kiMilli * error *
                         (int32_t)this->config.samplePeriodMs) / GAIN_MILLI_SCALE;

    this->integralMilliPerMille = clampIntegral(this->integralMilliPerMille + increment);
}

// Derivative on the measurement, not on the error. Differentiating the error
// would inject a one-sample spike of kd * deltaSetPoint / Ts straight into the
// motor on every set-point step - the classic derivative kick.
int32_t PidController::derivativeTerm(int16_t measuredDeci) const
{
    if (!this->hasPreviousMeasurement) return 0;

    int16_t change = (int16_t)(measuredDeci - this->previousMeasuredDeci);
    return -((int32_t)this->gains.kdMilli * change) /
            (int32_t)this->config.samplePeriodMs;
}

bool PidController::isWithinDeadBand(int16_t error) const
{
    int16_t magnitude = (error < 0) ? (int16_t)(-error) : error;
    return magnitude <= this->config.deadBandDeci;
}

int16_t PidController::clampPerMille(int32_t value)
{
    if (value > PER_MILLE_MAX) return PER_MILLE_MAX;
    if (value < -PER_MILLE_MAX) return (int16_t)(-PER_MILLE_MAX);
    return (int16_t)value;
}

int32_t PidController::clampIntegral(int32_t value)
{
    if (value > INTEGRAL_MILLI_MAX) return INTEGRAL_MILLI_MAX;
    if (value < -INTEGRAL_MILLI_MAX) return -INTEGRAL_MILLI_MAX;
    return value;
}

PidGains PidController::getGains() const      { return this->gains; }
PidTerms PidController::getTerms() const      { return this->terms; }
int16_t PidController::getSetPointDeci() const { return this->setPointDeci; }