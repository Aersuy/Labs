
#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H

#include <stdint.h>

#include "perMille.h"

// Gains in milli-units: 2.500 is stored as 2500. Three decimals because a
// realistic integral gain is around 0.050 and tenths cannot express it.
typedef struct {
    int16_t kpMilli;
    int16_t kiMilli;
    int16_t kdMilli;
} PidGains;

// Everything the console and the plotter want to see, so no caller has to
// recompute the split of the output into its three contributions.
typedef struct {
    int16_t errorDeciDeg;
    int16_t proportionalPerMille;
    int16_t integralPerMille;
    int16_t derivativePerMille;
    int16_t outputPerMille;
    bool isSaturated;
    bool isInDeadBand;
} PidTerms;

// Precondition: samplePeriodMs > 0 and setPointMin <= setPointMax. The caller
// builds this from constexpr literals at the application boundary, so it is a
// trusted input and is not re-validated at runtime.
typedef struct {
    int16_t setPointMin;
    int16_t setPointMax;
    int16_t deadBandDeciDeg;
    uint16_t samplePeriodMs;
} PidConfig;

// Discrete PID with a FIXED sample period: the caller guarantees update() is
// invoked once per samplePeriodMs, so dt never has to be measured and the
// class reads no clock. Deliberately free of Arduino.h and of all I/O, exactly
// like HysteresisController in lab 5.1 - the task that owns the motor applies
// the returned command, which keeps this deterministic and host-testable.
class PidController {
    public:
    PidController(const PidConfig &config, PidGains gains, int16_t setPointDeciDeg);

    bool setGains(const PidGains &gains);   // false if any gain is out of range
    bool setKp(int16_t kpMilli);
    bool setKi(int16_t kiMilli);
    bool setKd(int16_t kdMilli);
    bool setSetPoint(int16_t setPointDeciDeg);

    void reset();                           // clears integral and derivative memory

    int16_t update(int16_t measuredDeciDeg);        // -> output in per-mille

    PidGains getGains() const;
    PidTerms getTerms() const;
    int16_t getSetPointDeciDeg() const;

    static const int16_t GAIN_MILLI_MIN = 0;
    // Not 32767: kiMilli * error * samplePeriodMs must stay inside int32_t in
    // accumulateIntegral(). The application pins that with a static_assert,
    // because the bound depends on the control period it chooses.
    static const int16_t GAIN_MILLI_MAX = 20000;
    static const int32_t INTEGRAL_MILLI_MAX = 1000L * PER_MILLE_MAX;

    private:
    void computeTerms(int16_t error, int16_t measuredDeciDeg);
    void holdInDeadBand();
    void accumulateIntegral(int16_t error);
    int32_t proportionalTerm(int16_t error) const;
    int32_t derivativeTerm(int16_t measuredDeciDeg) const;
    bool isWithinDeadBand(int16_t error) const;

    static int16_t clampPerMille(int32_t value);
    static int32_t clampIntegral(int32_t value);

    PidConfig config;
    PidGains gains;
    int16_t setPointDeciDeg;
    int32_t integralMilliPerMille;
    int16_t previousMeasuredDeciDeg;
    bool hasPreviousMeasurement;
    PidTerms terms;
};

#endif