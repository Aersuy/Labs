#ifndef PACKET_CODEC_H
#define PACKET_CODEC_H

#include <stdint.h>

#include "binaryPacket.h"

// Intoarce lungimea totala a cadrului scris in frameOut, sau 0 daca
// payloadLength depaseste PACKET_MAX_PAYLOAD (cadrul nu se scrie deloc).
// frameOut trebuie sa aiba cel putin PACKET_MAX_LEN octeti.
uint8_t encodePacket(const BinaryPacket *packet, uint8_t *frameOut);

// Suma de control acopera si LEN, nu doar PAYLOAD: un LEN corupt ar schimba
// altfel doar unde se citeste suma, iar cadrul ar putea trece verificarea cu
// date deplasate.
uint8_t packetChecksum(const uint8_t *frame, uint8_t payloadLength);

const char *commandName(uint8_t command);

#endif
