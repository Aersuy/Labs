#include <unity.h>

#include "binaryPacket.h"
#include "packetPayload.h"

void setUp(void) {}
void tearDown(void) {}

static void test_temperature_roundtrip_keeps_the_value(void)
{
    uint8_t payload[TEMPERATURE_PAYLOAD_LEN];
    int16_t restored = 0;

    packTemperature(2375, payload);

    TEST_ASSERT_TRUE(unpackTemperature(payload, TEMPERATURE_PAYLOAD_LEN, &restored));
    TEST_ASSERT_EQUAL_INT16(2375, restored);
}

// Octetul inalt pleaca primul: ordinea trebuie sa fie aceeasi pe ambele placi,
// altfel 23,7 C ajunge ca -32 C.
static void test_temperature_is_big_endian(void)
{
    uint8_t payload[TEMPERATURE_PAYLOAD_LEN];

    packTemperature(0x1234, payload);

    TEST_ASSERT_EQUAL_HEX8(0x12, payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34, payload[1]);
}

static void test_negative_temperature_survives_the_roundtrip(void)
{
    uint8_t payload[TEMPERATURE_PAYLOAD_LEN];
    int16_t restored = 0;

    packTemperature(-125, payload);

    TEST_ASSERT_TRUE(unpackTemperature(payload, TEMPERATURE_PAYLOAD_LEN, &restored));
    TEST_ASSERT_EQUAL_INT16(-125, restored);
}

static void test_temperature_with_wrong_length_is_rejected(void)
{
    uint8_t payload[PACKET_MAX_PAYLOAD] = { 0x00, 0x01, 0x02, 0x03 };
    int16_t restored = 0x5AA5;

    TEST_ASSERT_FALSE(unpackTemperature(payload, 0, &restored));
    TEST_ASSERT_FALSE(unpackTemperature(payload, 3, &restored));
    TEST_ASSERT_EQUAL_INT16(0x5AA5, restored);        // nu s-a scris nimic
}

static void test_led_request_accepts_only_zero_and_one(void)
{
    uint8_t payload[LED_PAYLOAD_LEN];
    bool isOn = false;

    packLedRequest(true, payload);
    TEST_ASSERT_TRUE(unpackLedRequest(payload, LED_PAYLOAD_LEN, &isOn));
    TEST_ASSERT_TRUE(isOn);

    packLedRequest(false, payload);
    TEST_ASSERT_TRUE(unpackLedRequest(payload, LED_PAYLOAD_LEN, &isOn));
    TEST_ASSERT_FALSE(isOn);
}

// Un octet corupt care a trecut suma de control nu are voie sa comande iesirea.
static void test_led_request_rejects_other_values_and_lengths(void)
{
    uint8_t payload[PACKET_MAX_PAYLOAD] = { 0x02, 0x00, 0x00, 0x00 };
    bool isOn = false;

    TEST_ASSERT_FALSE(unpackLedRequest(payload, LED_PAYLOAD_LEN, &isOn));

    payload[0] = 0x01;
    TEST_ASSERT_FALSE(unpackLedRequest(payload, 0, &isOn));
    TEST_ASSERT_FALSE(unpackLedRequest(payload, 2, &isOn));
    TEST_ASSERT_FALSE(isOn);
}

static void test_stats_roundtrip_keeps_every_counter(void)
{
    SlaveStats sent = { 12, 3, 7, 200 };
    uint8_t payload[STATS_PAYLOAD_LEN];
    SlaveStats restored = { 0, 0, 0, 0 };

    packStats(&sent, payload);

    TEST_ASSERT_TRUE(unpackStats(payload, STATS_PAYLOAD_LEN, &restored));
    TEST_ASSERT_EQUAL_UINT8(12, restored.acceptedCount);
    TEST_ASSERT_EQUAL_UINT8(3, restored.fifoDroppedCount);
    TEST_ASSERT_EQUAL_UINT8(7, restored.lostCount);
    TEST_ASSERT_EQUAL_UINT8(200, restored.corruptedCount);
}

static void test_stats_with_wrong_length_is_rejected(void)
{
    uint8_t payload[PACKET_MAX_PAYLOAD] = { 1, 2, 3, 4 };
    SlaveStats restored = { 9, 9, 9, 9 };

    TEST_ASSERT_FALSE(unpackStats(payload, 3, &restored));
    TEST_ASSERT_EQUAL_UINT8(9, restored.acceptedCount);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_temperature_roundtrip_keeps_the_value);
    RUN_TEST(test_temperature_is_big_endian);
    RUN_TEST(test_negative_temperature_survives_the_roundtrip);
    RUN_TEST(test_temperature_with_wrong_length_is_rejected);
    RUN_TEST(test_led_request_accepts_only_zero_and_one);
    RUN_TEST(test_led_request_rejects_other_values_and_lengths);
    RUN_TEST(test_stats_roundtrip_keeps_every_counter);
    RUN_TEST(test_stats_with_wrong_length_is_rejected);
    return UNITY_END();
}
