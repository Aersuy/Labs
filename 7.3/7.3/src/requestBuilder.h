#ifndef REQUEST_BUILDER_H
#define REQUEST_BUILDER_H

#include <stdint.h>

#include "binaryPacket.h"

// Partea determinista a masterului: ce pachet pleaca si daca un pachet primit
// este chiar raspunsul la el.
BinaryPacket buildRequest(uint8_t command, uint8_t sequence, bool isLedOn);

// Filtru pe ID si pe SEQ: un pachet de la alt dispozitiv sau raspunsul
// intarziat la o cerere anterioara nu inchide asteptarea curenta.
bool isResponseToRequest(const BinaryPacket *response, const BinaryPacket *request);

#endif
