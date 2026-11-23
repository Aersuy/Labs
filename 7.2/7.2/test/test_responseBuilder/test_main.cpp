#include <unity.h>

#include "responseBuilder.h"

void setUp(void) {}
void tearDown(void) {}

static const int16_t CONTEXT_TEMPERATURE = 231;
static const SlaveStats CONTEXT_STATS = { 40, 1, 2, 3 };

static SlaveContext testContext(void)
{
    SlaveContext context;
    context.deviceId = DEVICE_ID_SLAVE;
    context.temperatureDeci = CONTEXT_TEMPERATURE;
    context.stats = CONTEXT_STATS;
    return context;
}

static BinaryPacket request(uint8_t command, uint8_t payloadLength, uint8_t payloadByte)
{
    BinaryPacket packet;
    packet.deviceId = DEVICE_ID_SLAVE;
    packet.sequence = 33;
    packet.command = command;
    packet.payloadLength = payloadLength;
    packet.payload[0] = payloadByte;
    return packet;
}

static SlaveResponse answer(const BinaryPacket *packet)
{
    SlaveContext context = testContext();
    return buildResponse(packet, &context);
}

// Masterul potriveste raspunsul cu cererea prin SEQ, deci SEQ trebuie sa se
// intoarca neschimbat, inclusiv pe NACK.
static void test_response_echoes_the_request_sequence(void)
{
    BinaryPacket ping = request(CMD_PING, 0, 0);
    BinaryPacket unknown = request(0x55, 0, 0);

    TEST_ASSERT_EQUAL_UINT8(33, answer(&ping).packet.sequence);
    TEST_ASSERT_EQUAL_UINT8(33, answer(&unknown).packet.sequence);
    TEST_ASSERT_EQUAL_HEX8(DEVICE_ID_SLAVE, answer(&ping).packet.deviceId);
}

static void test_ping_is_acknowledged_without_payload(void)
{
    BinaryPacket ping = request(CMD_PING, 0, 0);

    SlaveResponse response = answer(&ping);

    TEST_ASSERT_EQUAL_HEX8(RESP_ACK, response.packet.command);
    TEST_ASSERT_EQUAL_UINT8(0, response.packet.payloadLength);
    TEST_ASSERT_EQUAL(LED_ACTION_NONE, response.ledAction);
}

static void test_temperature_request_answers_with_the_context_value(void)
{
    BinaryPacket read = request(CMD_READ_TEMP, 0, 0);

    SlaveResponse response = answer(&read);

    TEST_ASSERT_EQUAL_HEX8(RESP_DATA, response.packet.command);
    TEST_ASSERT_EQUAL_UINT8(TEMPERATURE_PAYLOAD_LEN, response.packet.payloadLength);

    int16_t sent = 0;
    TEST_ASSERT_TRUE(unpackTemperature(response.packet.payload,
                                       response.packet.payloadLength, &sent));
    TEST_ASSERT_EQUAL_INT16(CONTEXT_TEMPERATURE, sent);
}

static void test_stats_request_answers_with_the_context_counters(void)
{
    BinaryPacket stats = request(CMD_GET_STATS, 0, 0);

    SlaveResponse response = answer(&stats);

    TEST_ASSERT_EQUAL_HEX8(RESP_DATA, response.packet.command);

    SlaveStats sent = { 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(unpackStats(response.packet.payload,
                                 response.packet.payloadLength, &sent));
    TEST_ASSERT_EQUAL_UINT8(CONTEXT_STATS.acceptedCount, sent.acceptedCount);
    TEST_ASSERT_EQUAL_UINT8(CONTEXT_STATS.corruptedCount, sent.corruptedCount);
}

// Comanda cu PAYLOAD: raspunsul confirma starea ceruta, iar actiunea asupra
// LED-ului este decisa aici si aplicata de apelant.
static void test_led_request_is_acknowledged_and_asks_for_the_action(void)
{
    BinaryPacket turnOn = request(CMD_SET_LED, LED_PAYLOAD_LEN, 1);
    BinaryPacket turnOff = request(CMD_SET_LED, LED_PAYLOAD_LEN, 0);

    SlaveResponse onResponse = answer(&turnOn);
    TEST_ASSERT_EQUAL_HEX8(RESP_ACK, onResponse.packet.command);
    TEST_ASSERT_EQUAL_UINT8(1, onResponse.packet.payload[0]);
    TEST_ASSERT_EQUAL(LED_ACTION_TURN_ON, onResponse.ledAction);

    SlaveResponse offResponse = answer(&turnOff);
    TEST_ASSERT_EQUAL_UINT8(0, offResponse.packet.payload[0]);
    TEST_ASSERT_EQUAL(LED_ACTION_TURN_OFF, offResponse.ledAction);
}

static void test_unknown_command_is_refused_with_a_reason(void)
{
    BinaryPacket unknown = request(0x55, 0, 0);

    SlaveResponse response = answer(&unknown);

    TEST_ASSERT_EQUAL_HEX8(RESP_NACK, response.packet.command);
    TEST_ASSERT_EQUAL_UINT8(NACK_PAYLOAD_LEN, response.packet.payloadLength);
    TEST_ASSERT_EQUAL_UINT8(NACK_BAD_COMMAND, response.packet.payload[0]);
    TEST_ASSERT_EQUAL(LED_ACTION_NONE, response.ledAction);
}

// Un LEN care nu se potriveste cu comanda inseamna flux desincronizat, nu o
// extensie a protocolului: iesirea nu se comanda si nu se trimit date.
static void test_wrong_payload_length_is_refused(void)
{
    BinaryPacket fatPing = request(CMD_PING, 2, 0);
    BinaryPacket emptyLed = request(CMD_SET_LED, 0, 0);

    SlaveResponse pingResponse = answer(&fatPing);
    TEST_ASSERT_EQUAL_HEX8(RESP_NACK, pingResponse.packet.command);
    TEST_ASSERT_EQUAL_UINT8(NACK_BAD_LENGTH, pingResponse.packet.payload[0]);

    SlaveResponse ledResponse = answer(&emptyLed);
    TEST_ASSERT_EQUAL_HEX8(RESP_NACK, ledResponse.packet.command);
    TEST_ASSERT_EQUAL_UINT8(NACK_BAD_LENGTH, ledResponse.packet.payload[0]);
    TEST_ASSERT_EQUAL(LED_ACTION_NONE, ledResponse.ledAction);
}

static void test_invalid_led_value_is_refused(void)
{
    BinaryPacket badValue = request(CMD_SET_LED, LED_PAYLOAD_LEN, 9);

    SlaveResponse response = answer(&badValue);

    TEST_ASSERT_EQUAL_HEX8(RESP_NACK, response.packet.command);
    TEST_ASSERT_EQUAL(LED_ACTION_NONE, response.ledAction);
}

static void test_fifo_full_nack_names_its_reason(void)
{
    BinaryPacket read = request(CMD_READ_TEMP, 0, 0);

    SlaveResponse response = buildFifoFullNack(&read, DEVICE_ID_SLAVE);

    TEST_ASSERT_EQUAL_HEX8(RESP_NACK, response.packet.command);
    TEST_ASSERT_EQUAL_UINT8(NACK_FIFO_FULL, response.packet.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(33, response.packet.sequence);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_response_echoes_the_request_sequence);
    RUN_TEST(test_ping_is_acknowledged_without_payload);
    RUN_TEST(test_temperature_request_answers_with_the_context_value);
    RUN_TEST(test_stats_request_answers_with_the_context_counters);
    RUN_TEST(test_led_request_is_acknowledged_and_asks_for_the_action);
    RUN_TEST(test_unknown_command_is_refused_with_a_reason);
    RUN_TEST(test_wrong_payload_length_is_refused);
    RUN_TEST(test_invalid_led_value_is_refused);
    RUN_TEST(test_fifo_full_nack_names_its_reason);
    return UNITY_END();
}
