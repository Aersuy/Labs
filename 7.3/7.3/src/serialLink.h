#ifndef SERIAL_LINK_H
#define SERIAL_LINK_H

#include <stdint.h>

// Interfata seriala dedicata protocolului, separata de canalul STDIO. Exista
// doua implementari ale aceluiasi antet - serialLinkHardware.cpp (USART1 pe
// Mega) si serialLinkSoftware.cpp (SoftwareSerial pe Uno) - iar platformio.ini
// alege una, deci codul de aplicatie nu are nici un #ifdef.
void linkBegin(uint32_t baud);

// Intoarce false cand nu mai este nimic de citit; nu blocheaza niciodata.
bool linkReadByte(uint8_t *byteOut);

void linkWriteBytes(const uint8_t *bytes, uint8_t count);

#endif
