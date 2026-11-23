#include <unity.h>

#include "packetCodec.h"

void setUp(void) {}
void tearDown(void) {}

static BinaryPacket samplePacket(void)
{
    BinaryPacket packet;
    packet.deviceId = DEVICE_ID_SLAVE;
    packet.sequence = 7;
    packet.command = CMD_GET_STATS;
    packet.payloadLength = 3;
    packet.payload[0] = 0x11;
    packet.payload[1] = 0x22;
    packet.payload[2] = 0x33;
    return packet;
}

static void test_encoded_packet_has_the_expected_envelope(void)
{
    BinaryPacket packet = samplePacket();
    uint8_t frame[PACKET_MAX_LEN];

    uint8_t frameLen = encodePacket(&packet, frame);

    TEST_ASSERT_EQUAL_UINT8(packet.payloadLength + PACKET_OVERHEAD, frameLen);
    TEST_ASSERT_EQUAL_HEX8(PACKET_START, frame[PACKET_START_OFFSET]);
    TEST_ASSERT_EQUAL_HEX8(DEVICE_ID_SLAVE, frame[PACKET_ID_OFFSET]);
    TEST_ASSERT_EQUAL_UINT8(7, frame[PACKET_SEQ_OFFSET]);
    TEST_ASSERT_EQUAL_HEX8(CMD_GET_STATS, frame[PACKET_CMD_OFFSET]);
    TEST_ASSERT_EQUAL_UINT8(3, frame[PACKET_LEN_OFFSET]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(packet.payload, frame + PACKET_PAYLOAD_OFFSET, 3);
    TEST_ASSERT_EQUAL_HEX8(PACKET_END, frame[frameLen - 1]);
}

static void test_empty_payload_is_a_valid_packet(void)
{
    BinaryPacket packet = samplePacket();
    packet.command = CMD_PING;
    packet.payloadLength = 0;
    uint8_t frame[PACKET_MAX_LEN];

    uint8_t frameLen = encodePacket(&packet, frame);

    TEST_ASSERT_EQUAL_UINT8(PACKET_OVERHEAD, frameLen);
    TEST_ASSERT_EQUAL_HEX8(PACKET_END, frame[PACKET_OVERHEAD - 1]);
}

static void test_payload_over_the_limit_is_refused(void)
{
    BinaryPacket packet = samplePacket();
    packet.payloadLength = PACKET_MAX_PAYLOAD + 1;
    uint8_t frame[PACKET_MAX_LEN + 1];
    frame[PACKET_START_OFFSET] = 0;

    TEST_ASSERT_EQUAL_UINT8(0, encodePacket(&packet, frame));
    TEST_ASSERT_EQUAL_HEX8(0, frame[PACKET_START_OFFSET]);   // nu s-a scris nimic
}

static void test_checksum_covers_the_header_fields_and_the_payload(void)
{
    BinaryPacket packet = samplePacket();
    uint8_t frame[PACKET_MAX_LEN];
    uint8_t frameLen = encodePacket(&packet, frame);

    uint8_t expected = (uint8_t)(packet.deviceId ^ packet.sequence ^ packet.command ^
                                 packet.payloadLength);
    for (uint8_t i = 0; i < packet.payloadLength; i++) expected ^= packet.payload[i];

    TEST_ASSERT_EQUAL_HEX8(expected, packetChecksum(frame, packet.payloadLength));
    TEST_ASSERT_EQUAL_HEX8(expected, frame[frameLen - 2]);
}

// Regresie: suma de control acopera si octetul LEN. Un LEN scurtat cu unu lasa
// PAYLOAD-ul si suma neatinse, deci numai includerea lui in suma prinde cazul.
static void test_shortened_length_changes_the_checksum(void)
{
    BinaryPacket packet = samplePacket();
    uint8_t frame[PACKET_MAX_LEN];
    encodePacket(&packet, frame);

    uint8_t honest = packetChecksum(frame, packet.payloadLength);
    frame[PACKET_LEN_OFFSET] = (uint8_t)(packet.payloadLength - 1);

    TEST_ASSERT_NOT_EQUAL(honest, packetChecksum(frame, packet.payloadLength - 1));
}

static void test_command_names_are_distinct_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("PING", commandName(CMD_PING));
    TEST_ASSERT_EQUAL_STRING("NACK", commandName(RESP_NACK));
    TEST_ASSERT_EQUAL_STRING("?", commandName(0x77));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_encoded_packet_has_the_expected_envelope);
    RUN_TEST(test_empty_payload_is_a_valid_packet);
    RUN_TEST(test_payload_over_the_limit_is_refused);
    RUN_TEST(test_checksum_covers_the_header_fields_and_the_payload);
    RUN_TEST(test_shortened_length_changes_the_checksum);
    RUN_TEST(test_command_names_are_distinct_strings);
    return UNITY_END();
}
