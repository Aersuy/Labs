// TEC1-12706 Peltier element, its plate, and the KY-013 thermistor bolted to
// that plate - all three as one simulated part.
//
// Wokwi has no thermoelectric element, and nothing in the simulator can change
// a wokwi-ntc-temperature-sensor's temperature, so a real NTC part could never
// respond to the relay. This chip stands in for the whole assembly instead: it
// senses whether the relay has put the supply across the element, models the
// plate warming or cooling away from the room, and reports the plate through
// OUT as the voltage a 10K NTC divider would produce at that temperature.
//
// That keeps the beta equation on the firmware side doing real work: the
// controller reads OUT with its ADC and converts it back, exactly as it would
// read a physical KY-013 on the bench.
//
//   PWR (digital in)  <- KY-019 NC contact; live when the coil is released
//   OUT (analog out)  -> plate temperature, as a KY-013 divider voltage
//
// The element's polarity is fixed by how it is wired, so a single supply line
// makes it either a heater or a cooler: the signed "rate" control chooses
// which, positive for heating.

#include "wokwi-api.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// The KY-013 is a 10K thermistor in series with a 10K resistor, beta 3950.
// These must stay identical to the constants in src/ntcSensor.cpp, or the two
// halves of the loop disagree by a constant offset.
static const float R_FIXED = 10000.0f;   // divider resistor, ohms
static const float R0 = 10000.0f;        // thermistor resistance at T0
static const float T0_KELVIN = 298.15f;  // 25 C
static const float BETA = 3950.0f;
static const float VREF = 5.0f;          // all virtual ADCs reference 5 V

static const float KELVIN_OFFSET = 273.15f;

// The plate cannot run away: a Peltier stalls once its own dissipation
// balances the heat it pumps, so clamp instead of integrating forever.
static const float PLATE_MIN_C = -10.0f;
static const float PLATE_MAX_C = 80.0f;

static const uint32_t TICK_MICROS = 50000;   // 20 Hz model update
static const float TICK_SECONDS = 0.05f;
static const uint32_t LOG_EVERY_TICKS = 20;  // one console line per second

typedef struct {
    pin_t pwrPin;
    pin_t outPin;

    uint32_t ambientAttr;
    uint32_t tauAttr;
    uint32_t rateAttr;

    float plateC;
    bool isPlateSeeded;
    uint32_t tickCount;
} chip_state_t;

static float clampFloat(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

// Temperature -> divider voltage: the inverse of what the firmware computes.
static float dividerVoltsFor(float celsius)
{
    float kelvin = clampFloat(celsius, PLATE_MIN_C, PLATE_MAX_C) + KELVIN_OFFSET;
    float resistance = R0 * expf(BETA * (1.0f / kelvin - 1.0f / T0_KELVIN));
    return VREF * resistance / (resistance + R_FIXED);
}

// The NC contact is live only while the coil is released. When the coil pulls
// COM over to NO this pin is left floating, so it is pulled down and reads as
// unpowered rather than as a stale high.
static bool isElementPowered(const chip_state_t *chip)
{
    return pin_read(chip->pwrPin) == HIGH;
}

static void chip_tick(void *user_data)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    float ambientC = attr_read_float(chip->ambientAttr);

    // Before the first tick the plate has no history: start it at the room it
    // is sitting in rather than at an arbitrary zero.
    if (!chip->isPlateSeeded)
    {
        chip->plateC = ambientC;
        chip->isPlateSeeded = true;
    }

    float tau = attr_read_float(chip->tauAttr);
    if (tau < 0.1f) tau = 0.1f;

    bool isPowered = isElementPowered(chip);
    float drive = isPowered ? attr_read_float(chip->rateAttr) : 0.0f;

    // First-order plate: it leaks toward the room on its time constant while
    // the element pumps heat in or out at a fixed rate.
    float leak = (ambientC - chip->plateC) / tau;
    chip->plateC = clampFloat(chip->plateC + TICK_SECONDS * (leak + drive),
                              PLATE_MIN_C, PLATE_MAX_C);

    pin_dac_write(chip->outPin, dividerVoltsFor(chip->plateC));

    if (++chip->tickCount >= LOG_EVERY_TICKS)
    {
        chip->tickCount = 0;
        printf("[peltier] amb=%.1fC plate=%.1fC element=%s\n",
               ambientC, chip->plateC, isPowered ? "ON" : "off");
    }
}

void chip_init(void)
{
    chip_state_t *chip = (chip_state_t *)calloc(1, sizeof(chip_state_t));
    if (chip == NULL) return;

    chip->pwrPin = pin_init("PWR", INPUT_PULLDOWN);
    chip->outPin = pin_init("OUT", ANALOG);

    chip->ambientAttr = attr_init_float("ambient", 24.0f);
    chip->tauAttr = attr_init_float("tau", 20.0f);
    chip->rateAttr = attr_init_float("rate", 0.5f);

    chip->isPlateSeeded = false;
    chip->tickCount = 0;

    const timer_config_t timerConfig = {
        .user_data = chip,
        .callback = chip_tick,
    };
    timer_t timer = timer_init(&timerConfig);
    timer_start(timer, TICK_MICROS, true);

    printf("[peltier] ready: TEC1-12706 + plate + KY-013\n");
}
