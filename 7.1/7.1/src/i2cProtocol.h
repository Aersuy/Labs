#ifndef I2C_PROTOCOL_H
#define I2C_PROTOCOL_H

#include <stdint.h>

// Contractul magistralei, singura sursa pentru ambele programe: o adresa
// schimbata doar pe o parte ar aparea ca "sclav mut", nu ca eroare de
// compilare, de aceea constantele nu se repeta in mainSlave/mainMaster.
//
// Adresa este in intervalul 7 biti liber (0x08 ... 0x77), deci nu se bate cu
// adresele rezervate ale magistralei.
constexpr uint8_t SLAVE_I2C_ADDRESS = 0x21;

// Masterul cere explicit o citire inainte de a asculta raspunsul. Orice alt
// cod primeste un cadru cu PAYLOAD gol, deci masterul deosebeste "comanda
// necunoscuta" de "magistrala moarta".
constexpr uint8_t CMD_READ_DISTANCES = 0x10;

#endif
