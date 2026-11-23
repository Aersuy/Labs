#include <unity.h>

#include "mqttBackoff.h"

void setUp(void) {}
void tearDown(void) {}

static void test_first_retry_waits_the_minimum(void)
{
    TEST_ASSERT_EQUAL_UINT32(MQTT_RECONNECT_DELAY_MIN_MS, reconnectDelayMs(0));
}

static void test_delay_doubles_with_each_failure(void)
{
    TEST_ASSERT_EQUAL_UINT32(2000, reconnectDelayMs(1));
    TEST_ASSERT_EQUAL_UINT32(4000, reconnectDelayMs(2));
    TEST_ASSERT_EQUAL_UINT32(8000, reconnectDelayMs(3));
    TEST_ASSERT_EQUAL_UINT32(16000, reconnectDelayMs(4));
}

// Dupa cateva esecuri intervalul se opreste: o placa fara retea trebuie sa
// reincerce rar, dar sa reincerce.
static void test_delay_stops_at_the_maximum(void)
{
    TEST_ASSERT_EQUAL_UINT32(MQTT_RECONNECT_DELAY_MAX_MS, reconnectDelayMs(5));
    TEST_ASSERT_EQUAL_UINT32(MQTT_RECONNECT_DELAY_MAX_MS, reconnectDelayMs(6));
}

static void test_saturated_counter_does_not_overflow(void)
{
    TEST_ASSERT_EQUAL_UINT32(MQTT_RECONNECT_DELAY_MAX_MS, reconnectDelayMs(255));
}

static void test_delay_never_leaves_the_allowed_range(void)
{
    for (uint8_t attempt = 0; attempt < 32; attempt++)
    {
        uint32_t delay = reconnectDelayMs(attempt);

        TEST_ASSERT_TRUE(delay >= MQTT_RECONNECT_DELAY_MIN_MS);
        TEST_ASSERT_TRUE(delay <= MQTT_RECONNECT_DELAY_MAX_MS);
    }
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_retry_waits_the_minimum);
    RUN_TEST(test_delay_doubles_with_each_failure);
    RUN_TEST(test_delay_stops_at_the_maximum);
    RUN_TEST(test_saturated_counter_does_not_overflow);
    RUN_TEST(test_delay_never_leaves_the_allowed_range);
    return UNITY_END();
}
