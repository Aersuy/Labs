#ifndef BINARY_PACKET_H
#define BINARY_PACKET_H

#include <stdint.h>

// Contractul protocolului logic binar, singura sursa pentru ambele programe:
// un cod de comanda schimbat doar pe o parte ar aparea ca "NACK", nu ca eroare
// de compilare, de aceea constantele nu se repeta in mainMaster/mainSlave.
//
//  indice  0     1    2     3     4     5 .. 4+LEN      5+LEN   6+LEN
//          STX   ID   SEQ   CMD   LEN   PAYLOAD         CHK     ETX
//
// LEN vine inaintea PAYLOAD-ului, deci analizorul numara octetii in loc sa
// caute delimitatori: un octet de date egal cu STX sau ETX nu poate fi
// confundat cu o margine de pachet si protocolul nu are nevoie de escapare.
constexpr uint8_t PACKET_START = 0x02;             // STX
constexpr uint8_t PACKET_END = 0x03;               // ETX

constexpr uint8_t PACKET_START_OFFSET = 0;
constexpr uint8_t PACKET_ID_OFFSET = 1;
constexpr uint8_t PACKET_SEQ_OFFSET = 2;
constexpr uint8_t PACKET_CMD_OFFSET = 3;
constexpr uint8_t PACKET_LEN_OFFSET = 4;
constexpr uint8_t PACKET_PAYLOAD_OFFSET = 5;

// Cate campuri acopera suma de control in afara PAYLOAD-ului: ID, SEQ, CMD, LEN.
constexpr uint8_t PACKET_HEADER_FIELDS = 4;
constexpr uint8_t PACKET_OVERHEAD = 7;             // STX ID SEQ CMD LEN CHK ETX

constexpr uint8_t PACKET_MAX_PAYLOAD = 4;
constexpr uint8_t PACKET_MAX_LEN = PACKET_MAX_PAYLOAD + PACKET_OVERHEAD;

// Identificatorul dispozitivului adresat, ca o adresa de magistrala: cererea
// il poarta pentru ca sclavul sa filtreze, raspunsul il poarta pentru ca
// masterul sa recunoasca emitentul.
constexpr uint8_t DEVICE_ID_SLAVE = 0x02;

// Cererile au nibble-ul inalt 0x1, raspunsurile il au peste 0x80: un cod de
// raspuns primit in locul unei cereri se vede imediat.
enum PacketCommand : uint8_t {
    CMD_PING = 0x10,
    CMD_READ_TEMP = 0x11,
    CMD_SET_LED = 0x12,
    CMD_GET_STATS = 0x13,
    RESP_ACK = 0x80,
    RESP_DATA = 0x81,
    RESP_NACK = 0xE0
};

enum NackReason : uint8_t {
    NACK_BAD_COMMAND = 1,
    NACK_BAD_LENGTH = 2,
    NACK_FIFO_FULL = 3
};

// Pachetul in forma logica, fara delimitatori si fara suma de control: acelea
// exista numai pe firul serial, nu in FIFO si nu in logica aplicatiei.
struct BinaryPacket {
    uint8_t deviceId;
    uint8_t sequence;
    uint8_t command;
    uint8_t payloadLength;
    uint8_t payload[PACKET_MAX_PAYLOAD];
};

#endif
