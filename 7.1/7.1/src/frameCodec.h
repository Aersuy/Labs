#ifndef FRAME_CODEC_H
#define FRAME_CODEC_H

#include <stdint.h>

// Anvelopa ceruta de enunt: HEAD, LENGTH, PAYLOAD, CHECKSUM. Modulul nu stie
// ce inseamna octetii din PAYLOAD - despre asta se ocupa distancePayload.
//
//   indice   0      1         2 .. 1+LENGTH      2+LENGTH
//            HEAD   LENGTH    PAYLOAD            CHECKSUM
constexpr uint8_t FRAME_HEAD = 0xA5;
constexpr uint8_t FRAME_HEAD_OFFSET = 0;
constexpr uint8_t FRAME_LENGTH_OFFSET = 1;
constexpr uint8_t FRAME_PAYLOAD_OFFSET = 2;
constexpr uint8_t FRAME_OVERHEAD = 3;             // HEAD + LENGTH + CHECKSUM

// Tamponul TWI din nucleul Arduino are 32 de octeti, deci un cadru trebuie sa
// incapa intreg intr-un singur transfer.
constexpr uint8_t FRAME_MAX_PAYLOAD = 12;
constexpr uint8_t FRAME_MAX_LEN = FRAME_MAX_PAYLOAD + FRAME_OVERHEAD;

enum FrameStatus : uint8_t {
    FRAME_OK = 0,
    FRAME_TRUNCATED,        // au venit mai putini octeti decat anunta LENGTH
    FRAME_BAD_HEAD,
    FRAME_BAD_LENGTH,       // LENGTH peste maximul protocolului
    FRAME_BAD_CHECKSUM
};

// Intoarce lungimea totala a cadrului scris in frameOut, sau 0 daca
// payloadLen depaseste FRAME_MAX_PAYLOAD (cadrul nu se scrie deloc).
uint8_t encodeFrame(const uint8_t *payload, uint8_t payloadLen, uint8_t *frameOut);

// Verifica anvelopa si copiaza PAYLOAD-ul. payloadOut trebuie sa aiba cel
// putin FRAME_MAX_PAYLOAD octeti; se scrie numai cand se intoarce FRAME_OK.
FrameStatus decodeFrame(const uint8_t *frame, uint8_t frameLen,
                        uint8_t *payloadOut, uint8_t *payloadLenOut);

uint8_t frameChecksum(const uint8_t *frame, uint8_t payloadLen);
const char *frameStatusName(FrameStatus status);

#endif
