#ifndef PERMILLE_H
#define PERMILLE_H

#include <stdint.h>

// The controller output and every actuator command in this project are
// expressed as per-mille of full scale. One source of truth, so the control
// law and the actuator mapping cannot drift apart.
const int16_t PER_MILLE_MAX = 1000;

#endif