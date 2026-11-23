#ifndef I2C_MASTER_LINK_H
#define I2C_MASTER_LINK_H

#include <stdint.h>

// Rezultatul interogarii, inainte de verificarea anvelopei: aici se vede doar
// daca sclavul a raspuns pe magistrala, nu daca datele sunt corecte.
enum QueryStatus : uint8_t {
    QUERY_OK = 0,
    QUERY_COMMAND_NOT_ACKED,   // sclavul nu confirma adresa (cablaj, pull-up, adresa)
    QUERY_SHORT_ANSWER         // a confirmat, dar a trimis mai putini octeti
};

void i2cMasterLinkBegin();

// Trimite comanda de citire si preia raspunsul. frameOut trebuie sa aiba cel
// putin expectedLen octeti; receivedLenOut primeste mereu cati au sosit.
QueryStatus queryDistanceFrame(uint8_t *frameOut, uint8_t expectedLen,
                               uint8_t *receivedLenOut);

const char *queryStatusName(QueryStatus status);

#endif
