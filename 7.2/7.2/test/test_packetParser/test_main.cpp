#include <unity.h>

#include "packetCodec.h"
#include "packetParser.h"

void setUp(void) {}
void tearDown(void) {}

static PacketParser parser;
static BinaryPacket decoded;

static BinaryPacket packetWithPayload(const uint8_t *payload, uint8_t payloadLength)
{
    BinaryPacket packet;
    packet.deviceId = DEVICE_ID_SLAVE;
    packet.sequence = 42;
    packet.command = CMD_READ_TEMP;
    packet.payloadLength = payloadLength;
    for (uint8_t i = 0; i < payloadLength; i++) packet.payload[i] = payload[i];
    return packet;
}

// Trimite toti octetii in afara de ultimul si cere ca niciunul sa nu incheie
// pachetul; intoarce rezultatul ultimului octet.
static ParseResult feedWholeFrame(const uint8_t *frame, uint8_t frameLen)
{
    for (uint8_t i = 0; i < (uint8_t)(frameLen - 1); i++)
    {
        TEST_ASSERT_EQUAL(PARSE_NEED_MORE, parserFeed(&parser, frame[i], &decoded));
    }
    return parserFeed(&parser, frame[frameLen - 1], &decoded);
}

static uint8_t buildSampleFrame(uint8_t *frameOut)
{
    const uint8_t payload[] = { 0x7F, 0x80 };
    BinaryPacket packet = packetWithPayload(payload, 2);
    return encodePacket(&packet, frameOut);
}

static void setUpParser(void)
{
    parserReset(&parser);
}

static void test_roundtrip_returns_the_same_packet(void)
{
    setUpParser();
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);

    TEST_ASSERT_EQUAL(PARSE_DONE, feedWholeFrame(frame, frameLen));
    TEST_ASSERT_EQUAL_HEX8(DEVICE_ID_SLAVE, decoded.deviceId);
    TEST_ASSERT_EQUAL_UINT8(42, decoded.sequence);
    TEST_ASSERT_EQUAL_HEX8(CMD_READ_TEMP, decoded.command);
    TEST_ASSERT_EQUAL_UINT8(2, decoded.payloadLength);
    TEST_ASSERT_EQUAL_HEX8(0x7F, decoded.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80, decoded.payload[1]);
}

// Regresie pentru decizia de a nu escapa nimic: PAYLOAD-ul poate contine chiar
// octetii STX si ETX, iar analizorul numara in loc sa caute delimitatori.
static void test_payload_may_contain_the_delimiter_bytes(void)
{
    setUpParser();
    const uint8_t payload[] = { PACKET_START, PACKET_END, PACKET_START, PACKET_END };
    BinaryPacket packet = packetWithPayload(payload, 4);
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = encodePacket(&packet, frame);

    TEST_ASSERT_EQUAL(PARSE_DONE, feedWholeFrame(frame, frameLen));
    TEST_ASSERT_EQUAL_UINT8(4, decoded.payloadLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, decoded.payload, 4);
}

static void test_empty_payload_packet_is_accepted(void)
{
    setUpParser();
    BinaryPacket packet = packetWithPayload(NULL, 0);
    packet.command = CMD_PING;
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = encodePacket(&packet, frame);

    TEST_ASSERT_EQUAL(PARSE_DONE, feedWholeFrame(frame, frameLen));
    TEST_ASSERT_EQUAL_UINT8(0, decoded.payloadLength);
    TEST_ASSERT_EQUAL_HEX8(CMD_PING, decoded.command);
}

// Zgomotul de pe fir dinaintea unui STX nu este o eroare, ci octeti care se
// arunca: altfel fiecare pornire de placa ar raporta un pachet corupt.
static void test_leading_noise_is_skipped(void)
{
    setUpParser();
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);

    TEST_ASSERT_EQUAL(PARSE_NEED_MORE, parserFeed(&parser, 0xFF, &decoded));
    TEST_ASSERT_EQUAL(PARSE_NEED_MORE, parserFeed(&parser, 0x41, &decoded));
    TEST_ASSERT_EQUAL(PARSE_DONE, feedWholeFrame(frame, frameLen));
}

static void test_flipped_payload_bit_is_caught(void)
{
    setUpParser();
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);
    frame[PACKET_PAYLOAD_OFFSET] ^= 0x01;

    TEST_ASSERT_EQUAL(PARSE_BAD_CHECKSUM, feedWholeFrame(frame, frameLen - 1));
}

static void test_length_over_the_limit_is_rejected_before_the_payload(void)
{
    setUpParser();
    uint8_t frame[PACKET_MAX_LEN];
    buildSampleFrame(frame);
    frame[PACKET_LEN_OFFSET] = PACKET_MAX_PAYLOAD + 1;

    TEST_ASSERT_EQUAL(PARSE_BAD_LENGTH, feedWholeFrame(frame, PACKET_PAYLOAD_OFFSET));
}

static void test_missing_end_delimiter_is_rejected(void)
{
    setUpParser();
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);
    frame[frameLen - 1] = 0x00;

    TEST_ASSERT_EQUAL(PARSE_BAD_END, feedWholeFrame(frame, frameLen));
}

// Dupa orice eroare analizorul trebuie sa fie gata pentru pachetul urmator:
// altfel o singura coliziune pe fir ar bloca legatura pentru totdeauna.
static void test_parser_resyncs_after_a_broken_packet(void)
{
    setUpParser();
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);

    uint8_t broken[PACKET_MAX_LEN];
    for (uint8_t i = 0; i < frameLen; i++) broken[i] = frame[i];
    broken[frameLen - 1] = 0x00;
    TEST_ASSERT_EQUAL(PARSE_BAD_END, feedWholeFrame(broken, frameLen));

    TEST_ASSERT_EQUAL(PARSE_DONE, feedWholeFrame(frame, frameLen));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip_returns_the_same_packet);
    RUN_TEST(test_payload_may_contain_the_delimiter_bytes);
    RUN_TEST(test_empty_payload_packet_is_accepted);
    RUN_TEST(test_leading_noise_is_skipped);
    RUN_TEST(test_flipped_payload_bit_is_caught);
    RUN_TEST(test_length_over_the_limit_is_rejected_before_the_payload);
    RUN_TEST(test_missing_end_delimiter_is_rejected);
    RUN_TEST(test_parser_resyncs_after_a_broken_packet);
    return UNITY_END();
}
