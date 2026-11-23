#include <unity.h>

#include "requestBuilder.h"
#include "packetPayload.h"

void setUp(void) {}
void tearDown(void) {}

static void test_request_without_data_has_no_payload(void)
{
    BinaryPacket request = buildRequest(CMD_PING, 5, false);

    TEST_ASSERT_EQUAL_HEX8(DEVICE_ID_SLAVE, request.deviceId);
    TEST_ASSERT_EQUAL_UINT8(5, request.sequence);
    TEST_ASSERT_EQUAL_HEX8(CMD_PING, request.command);
    TEST_ASSERT_EQUAL_UINT8(0, request.payloadLength);
}

static void test_led_request_carries_the_wanted_state(void)
{
    BinaryPacket turnOn = buildRequest(CMD_SET_LED, 6, true);
    BinaryPacket turnOff = buildRequest(CMD_SET_LED, 7, false);

    TEST_ASSERT_EQUAL_UINT8(LED_PAYLOAD_LEN, turnOn.payloadLength);
    TEST_ASSERT_EQUAL_UINT8(1, turnOn.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(0, turnOff.payload[0]);
}

// Starea ceruta pentru LED nu are inteles pentru celelalte comenzi: un PAYLOAD
// nenul pe PING ar fi refuzat de sclav cu NACK.
static void test_led_state_does_not_leak_into_other_requests(void)
{
    BinaryPacket request = buildRequest(CMD_GET_STATS, 8, true);

    TEST_ASSERT_EQUAL_UINT8(0, request.payloadLength);
}

static void test_matching_response_is_recognised(void)
{
    BinaryPacket request = buildRequest(CMD_READ_TEMP, 9, false);
    BinaryPacket response = request;
    response.command = RESP_DATA;

    TEST_ASSERT_TRUE(isResponseToRequest(&response, &request));
}

// Filtru pe ID: un pachet de la alt dispozitiv de pe aceeasi legatura nu
// inchide asteptarea.
static void test_response_from_another_device_is_ignored(void)
{
    BinaryPacket request = buildRequest(CMD_READ_TEMP, 9, false);
    BinaryPacket response = request;
    response.deviceId = 0x7A;

    TEST_ASSERT_FALSE(isResponseToRequest(&response, &request));
}

// Ecoul intarziat al unei cereri anterioare poarta alt SEQ: daca ar fi acceptat,
// masterul ar afisa o temperatura veche ca si cum ar fi proaspata.
static void test_response_to_an_older_request_is_ignored(void)
{
    BinaryPacket request = buildRequest(CMD_READ_TEMP, 9, false);
    BinaryPacket response = request;
    response.sequence = 8;

    TEST_ASSERT_FALSE(isResponseToRequest(&response, &request));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_request_without_data_has_no_payload);
    RUN_TEST(test_led_request_carries_the_wanted_state);
    RUN_TEST(test_led_state_does_not_leak_into_other_requests);
    RUN_TEST(test_matching_response_is_recognised);
    RUN_TEST(test_response_from_another_device_is_ignored);
    RUN_TEST(test_response_to_an_older_request_is_ignored);
    return UNITY_END();
}
