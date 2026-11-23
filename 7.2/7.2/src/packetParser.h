#ifndef PACKET_PARSER_H
#define PACKET_PARSER_H

#include <stdint.h>

#include "binaryPacket.h"

// Pe UART pachetul soseste ca flux de octeti, nu ca transfer delimitat de
// magistrala: analizorul este o masina de stari care primeste cate un octet si
// anunta cand un pachet intreg a fost verificat. Starea sta in apelant, deci
// modulul ramane determinist si se poate testa pe calculator.
enum ParserState : uint8_t {
    PARSER_WAIT_START,
    PARSER_HEADER,
    PARSER_PAYLOAD,
    PARSER_CHECKSUM,
    PARSER_END
};

enum ParseResult : uint8_t {
    PARSE_NEED_MORE = 0,
    PARSE_DONE,
    PARSE_BAD_LENGTH,          // LEN peste maximul protocolului
    PARSE_BAD_CHECKSUM,
    PARSE_BAD_END              // ETX lipsa la pozitia anuntata de LEN
};

struct PacketParser {
    uint8_t state;
    uint8_t index;                   // cati octeti din raw sunt completati
    uint8_t raw[PACKET_MAX_LEN];
};

void parserReset(PacketParser *parser);

// packetOut se scrie numai cand se intoarce PARSE_DONE. Dupa orice rezultat
// diferit de PARSE_NEED_MORE analizorul este deja resincronizat: urmatorul
// octet este iar cautat ca STX.
ParseResult parserFeed(PacketParser *parser, uint8_t byte, BinaryPacket *packetOut);

const char *parseResultName(ParseResult result);

#endif
