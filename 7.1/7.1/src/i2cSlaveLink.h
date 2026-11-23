#ifndef I2C_SLAVE_LINK_H
#define I2C_SLAVE_LINK_H

#include <stdint.h>

// Deschide magistrala in mod sclav si leaga cele doua apeluri Wire. Se apeleaza
// dupa ce exista deja un cadru publicat, altfel o cerere sosita imediat ar
// primi zero octeti.
void i2cSlaveLinkBegin();

// Cate comenzi necunoscute au sosit de la pornire. Citita din task, incrementata
// in ISR, deci accesul este protejat.
uint16_t i2cSlaveUnknownCommandCount();

#endif
