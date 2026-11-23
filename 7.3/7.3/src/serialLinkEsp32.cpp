#include <Arduino.h>

#include "serialLink.h"

// ESP32 are trei UART-uri: UART0 duce jurnalul pe USB, deci legatura cu nodul
// sta pe UART2, cu receptie pe intrerupere si tampon de 256 de octeti - un
// pachet intreg (cel mult 11 octeti) incape de multe ori.
//
// GPIO16/GPIO17 sunt alese pentru ca sunt libere pe placile DevKit obisnuite.
// Important la cablare: TX-ul nodului este de 5 V si intra prin divizor
// (1k8 / 3k3) in RX-ul de aici, care nu suporta decat 3,3 V; in sens invers
// 3,3 V sunt citite corect de intrarea de 5 V a nodului, deci acolo firul merge
// direct.
constexpr int8_t LINK_RX_PIN = 16;
constexpr int8_t LINK_TX_PIN = 17;

void linkBegin(uint32_t baud)
{
    Serial2.begin(baud, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
}

bool linkReadByte(uint8_t *byteOut)
{
    if (Serial2.available() <= 0) return false;

    *byteOut = (uint8_t)Serial2.read();
    return true;
}

void linkWriteBytes(const uint8_t *bytes, uint8_t count)
{
    Serial2.write(bytes, count);
}
