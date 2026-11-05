// The whole actuator side of lab 5.2 as one simulated part: the L293D
// half-bridge, the TT gearmotor behind it, and the WYGH206 slot sensor
// reading the 20-slot disc on the output shaft.
//
// Wokwi has no DC motor, and nothing in the simulator can spin an encoder, so
// the loop would be open and the PID would have nothing to close. This chip
// stands in for the whole drivetrain instead: it decodes the three bridge
// lines exactly as an L293D does, integrates a first-order motor into a shaft
// speed, and emits the pulse train that speed would produce on OUT.
//
// That keeps the firmware side doing real work: the ISR counts edges and
// speedScale converts them back to deci-RPM, exactly as it would with a
// physical disc spinning in the sensor's slot.
//
//   IN1, IN2 (digital in)   <- direction, same truth table as the L293D
//   EN       (digital in)   <- analogWrite duty, measured over a window
//   OUT      (digital out)  -> one pulse per slot, as the LM393 produces
//
//   IN1 IN2   with EN duty > 0
//    1   0    forward  (shaft turns, pulses)
//    0   1    backward (shaft turns, pulses - the sensor cannot tell)
//    0   0    coast    (shaft free, friction only)
//    1   1    brake    (terminals shorted, fast decay)
//
// The sensor is single-channel on purpose: like the real one, it reports that
// the shaft moved and never which way, so a firmware bug that drives the
// motor backwards looks exactly like correct forward motion here too.

#include "wokwi-api.h"

#include <stdio.h>
#include <stdlib.h>

// Slots in the disc. Must stay identical to ENCODER_PULSES_PER_REV in
// src/speedScale.h, or the two halves of the loop disagree by a constant.
static const float PULSES_PER_REV = 20.0f;

static const uint32_t TICK_MICROS = 200;     // 5 kHz, so pulse edges land within 0.2 ms
static const float TICK_SECONDS = 0.0002f;
static const uint32_t LOG_EVERY_TICKS = 5000; // one console line per second

// A shorted motor stops much faster than a free one; both are expressed as
// multiples of the driven time constant, so a single control tunes all three.
static const float BRAKE_TAU_FACTOR = 0.3f;
static const float COAST_TAU_FACTOR = 4.0f;

// The slot is open for about half a revolution of the disc's pitch, so the
// pulse is a square wave rather than a spike. The firmware triggers on the
// rising edge, but the ISR's noise guard rejects edges closer than 1 ms, so
// the low half has to stay comfortably wider than that at full speed.
static const float PULSE_HIGH_FRACTION = 0.5f;

typedef enum {
    DRIVE_COAST = 0,
    DRIVE_FORWARD,
    DRIVE_BACKWARD,
    DRIVE_BRAKE
} drive_mode_t;

typedef struct {
    pin_t in1Pin;
    pin_t in2Pin;
    pin_t enPin;
    pin_t outPin;

    uint32_t speedAttr;
    uint32_t stictionAttr;
    uint32_t tauAttr;
    uint32_t loadAttr;

    // PWM on EN is far faster than one model tick, so the duty is integrated
    // from the edges rather than sampled, which would alias badly.
    uint32_t enLevel;
    uint64_t lastEdgeNanos;
    uint64_t windowStartNanos;
    uint64_t highNanos;

    float speedRpm;
    // Position within one slot pitch, 0..1. The pulse train is generated from
    // the shaft angle rather than from a free-running timer, so every change
    // of speed shows up in the very next pulse interval - which is the whole
    // quantity the firmware is trying to measure.
    float slotPhase;
    uint32_t tickCount;
} chip_state_t;

static float clampFloat(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float absFloat(float value)
{
    return (value < 0.0f) ? -value : value;
}

static void accumulateEnableTime(chip_state_t *chip, uint64_t now)
{
    if (chip->enLevel == HIGH) chip->highNanos += now - chip->lastEdgeNanos;
    chip->lastEdgeNanos = now;
}

static void onEnableChange(void *user_data, pin_t pin, uint32_t value)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    accumulateEnableTime(chip, get_sim_nanos());
    chip->enLevel = value;
}

// Fraction of the last window the bridge was enabled for. Closing the window
// over the level held since the last edge is what makes a steady HIGH read as
// 100 % and a steady LOW as 0 %, with no edges in the window at all.
static float closeDutyWindow(chip_state_t *chip)
{
    uint64_t now = get_sim_nanos();

    accumulateEnableTime(chip, now);

    uint64_t elapsed = now - chip->windowStartNanos;
    float duty = (elapsed > 0) ? (float)chip->highNanos / (float)elapsed : 0.0f;

    chip->highNanos = 0;
    chip->windowStartNanos = now;
    return clampFloat(duty, 0.0f, 1.0f);
}

static drive_mode_t readDriveMode(const chip_state_t *chip)
{
    bool isIn1High = pin_read(chip->in1Pin) == HIGH;
    bool isIn2High = pin_read(chip->in2Pin) == HIGH;

    if (isIn1High && !isIn2High) return DRIVE_FORWARD;
    if (!isIn1High && isIn2High) return DRIVE_BACKWARD;
    if (isIn1High && isIn2High)  return DRIVE_BRAKE;
    return DRIVE_COAST;
}

static const char *driveModeName(drive_mode_t mode)
{
    switch (mode)
    {
        case DRIVE_FORWARD:  return "FWD  ";
        case DRIVE_BACKWARD: return "BWD  ";
        case DRIVE_BRAKE:    return "BRAKE";
        case DRIVE_COAST:
        default:             return "coast";
    }
}

// Below the stiction floor the bridge only makes the gearbox hum: the shaft
// does not break away at all. This is the physical fact MOTOR_MIN_MOVE_PERCENT
// in the firmware exists to work around, so the chip has to model it or that
// remap would look like dead code.
static float targetSpeedFor(const chip_state_t *chip, drive_mode_t mode, float duty)
{
    if (mode == DRIVE_COAST || mode == DRIVE_BRAKE) return 0.0f;

    float stiction = attr_read_float(chip->stictionAttr) / 100.0f;
    if (duty < stiction) return 0.0f;

    // The load is an outside drag on the shaft, not something the motor
    // produces: a constant the proportional term alone cannot null, which is
    // what makes ki visible on the bench.
    float magnitude = attr_read_float(chip->speedAttr) * duty -
                      attr_read_float(chip->loadAttr);
    if (magnitude < 0.0f) magnitude = 0.0f;

    return (mode == DRIVE_FORWARD) ? magnitude : -magnitude;
}

static float timeConstantFor(const chip_state_t *chip, drive_mode_t mode)
{
    float tau = attr_read_float(chip->tauAttr);
    if (tau < 0.01f) tau = 0.01f;

    if (mode == DRIVE_BRAKE) return tau * BRAKE_TAU_FACTOR;
    if (mode == DRIVE_COAST) return tau * COAST_TAU_FACTOR;
    return tau;
}

// One slot pitch per 1/PULSES_PER_REV of a revolution, in either direction:
// the sensor sees the disc pass, not where it came from.
static void advanceSlotPhase(chip_state_t *chip)
{
    float revsThisTick = absFloat(chip->speedRpm) * TICK_SECONDS / 60.0f;

    chip->slotPhase += revsThisTick * PULSES_PER_REV;
    while (chip->slotPhase >= 1.0f) chip->slotPhase -= 1.0f;

    pin_write(chip->outPin, (chip->slotPhase < PULSE_HIGH_FRACTION) ? HIGH : LOW);
}

static void chip_tick(void *user_data)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    float duty = closeDutyWindow(chip);
    drive_mode_t mode = readDriveMode(chip);

    float target = targetSpeedFor(chip, mode, duty);
    float tau = timeConstantFor(chip, mode);

    chip->speedRpm += (target - chip->speedRpm) * (TICK_SECONDS / tau);

    advanceSlotPhase(chip);

    if (++chip->tickCount >= LOG_EVERY_TICKS)
    {
        chip->tickCount = 0;
        printf("[motorrig] %s duty=%3.0f%% n=%+6.1f rpm\n",
               driveModeName(mode), duty * 100.0f, chip->speedRpm);
    }
}

void chip_init(void)
{
    chip_state_t *chip = (chip_state_t *)calloc(1, sizeof(chip_state_t));
    if (chip == NULL) return;

    // The bridge inputs idle low on a Mega that has not run setup() yet, so a
    // pull-down keeps the first ticks reading "coast" instead of floating.
    chip->in1Pin = pin_init("IN1", INPUT_PULLDOWN);
    chip->in2Pin = pin_init("IN2", INPUT_PULLDOWN);
    chip->enPin = pin_init("EN", INPUT_PULLDOWN);
    chip->outPin = pin_init("OUT", OUTPUT_LOW);

    chip->speedAttr = attr_init_float("speed", 240.0f);
    chip->stictionAttr = attr_init_float("stiction", 18.0f);
    chip->tauAttr = attr_init_float("tau", 0.25f);
    chip->loadAttr = attr_init_float("load", 0.0f);

    chip->enLevel = pin_read(chip->enPin);
    chip->lastEdgeNanos = get_sim_nanos();
    chip->windowStartNanos = chip->lastEdgeNanos;
    chip->highNanos = 0;

    chip->speedRpm = 0.0f;
    chip->slotPhase = 0.0f;
    chip->tickCount = 0;

    const pin_watch_config_t watchConfig = {
        .user_data = chip,
        .edge = BOTH,
        .pin_change = onEnableChange,
    };
    pin_watch(chip->enPin, &watchConfig);

    const timer_config_t timerConfig = {
        .user_data = chip,
        .callback = chip_tick,
    };
    timer_t timer = timer_init(&timerConfig);
    timer_start(timer, TICK_MICROS, true);

    printf("[motorrig] ready: L293D + TT gearmotor + WYGH206 slot encoder\n");
}
