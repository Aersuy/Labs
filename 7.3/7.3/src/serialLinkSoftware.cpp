#include <Arduino.h>
#include <SoftwareSerial.h>

#include "serialLink.h"

// Uno are un singur USART si acela duce STDIO pe USB, deci legatura dintre
// placi este bit-bang pe doi pini obisnuiti. D0/D1 raman libere pentru UART-ul
// hardware.
constexpr uint8_t LINK_RX_PIN = 10;
constexpr uint8_t LINK_TX_PIN = 11;

static SoftwareSerial linkPort(LINK_RX_PIN, LINK_TX_PIN);

// SoftwareSerial tine intreruperile inchise cat timp emite (circa 1 ms pe
// octet la 9600), deci nu poate emite si primi in acelasi timp. Protocolul este
// strict cerere -> raspuns, deci placa nu transmite niciodata cand asteapta
// date: asta este invariantul care face legatura software suficienta aici.
void linkBegin(uint32_t baud)
{
    linkPort.begin(baud);
}

bool linkReadByte(uint8_t *byteOut)
{
    if (linkPort.available() <= 0) return false;

    *byteOut = (uint8_t)linkPort.read();
    return true;
}

void linkWriteBytes(const uint8_t *bytes, uint8_t count)
{
    linkPort.write(bytes, count);
}
