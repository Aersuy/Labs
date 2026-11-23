#include <Arduino.h>

#include "serialLink.h"

// USART1 al placii Mega: RX1 = D19, TX1 = D18. Receptia este pe intrerupere,
// cu tamponul de 64 de octeti al nucleului Arduino, deci un pachet intreg
// (cel mult 11 octeti) incape chiar daca taskul de receptie intarzie un tact.
void linkBegin(uint32_t baud)
{
    Serial1.begin(baud);
}

bool linkReadByte(uint8_t *byteOut)
{
    if (Serial1.available() <= 0) return false;

    *byteOut = (uint8_t)Serial1.read();
    return true;
}

void linkWriteBytes(const uint8_t *bytes, uint8_t count)
{
    Serial1.write(bytes, count);
}
