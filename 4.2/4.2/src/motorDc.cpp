#include "motorDc.h"

// constructor, inits the pins, sets the power to 0 and sets the direction
MotorDc::MotorDc(int in1Pin, int in2Pin, int enablePin)
    : channel(in1Pin, in2Pin, enablePin)
{
    this->power = 0;
    this->lastDirectionSign = 1;
}

// inits the channel the power for redundancy
void MotorDc::begin()
{
    this->channel.begin();
    this->power = 0;
    this->applyPower();
}

uint8_t MotorDc::percentToDuty(int absolutePercent)
{
    long duty = ((long)absolutePercent * L298Channel::PWM_DUTY_MAX)
                / MotorDc::POWER_MAX;
    return (uint8_t)duty;
}


// applies the logical power state to the physical motor
// using the lower level driver
void MotorDc::applyPower()
{
    if (this->power == 0)
    {
        this->channel.coast();
        return;
    }

    L298Direction direction = (this->power > 0) ? L298_DIR_FORWARD
                                                : L298_DIR_BACKWARD;
    this->lastDirectionSign = (this->power > 0) ? 1 : -1;

    this->channel.setDuty(0);
    this->channel.setDirection(direction);
    this->channel.setDuty(MotorDc::percentToDuty(abs(this->power)));
}

// setter for power, also applies
bool MotorDc::setPower(int percent)
{
    if (percent < MotorDc::POWER_MIN || percent > MotorDc::POWER_MAX)
    {
        return false;
    }
    this->power = percent;
    this->applyPower();
    return true;
}

// sets power to 0
void MotorDc::stop()
{
    this->power = 0;
    this->applyPower();
}

// sets power to max, doesn't change direction
void MotorDc::setMaxPower()
{
    this->power = MotorDc::POWER_MAX * this->lastDirectionSign;
    this->applyPower();
}

// increases the power by POWER_STEP, default 10%
bool MotorDc::increasePower()
{
    if (this->power >= MotorDc::POWER_MAX) return false;

    int next = this->power + MotorDc::POWER_STEP;
    if (next > MotorDc::POWER_MAX) next = MotorDc::POWER_MAX;
    return this->setPower(next);
}

bool MotorDc::decreasePower()
{
    if (this->power <= MotorDc::POWER_MIN) return false;

    int next = this->power - MotorDc::POWER_STEP;
    if (next < MotorDc::POWER_MIN) next = MotorDc::POWER_MIN;
    return this->setPower(next);
}

int MotorDc::getPower() const
{
    return this->power;
}

const char *MotorDc::directionName() const
{
    if (this->power > 0) return "FORWARD";
    if (this->power < 0) return "BACKWARD";
    return "STOP";
}