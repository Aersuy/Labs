#include <unity.h>
#include <string.h>

#include "rpcResponse.h"

void setUp(void) {}
void tearDown(void) {}

// Confirmarea trebuie sa plece pe exact id-ul cererii: pe oricare altul,
// widgetul de comutator ramane in asteptare.
static void test_reply_topic_carries_the_request_id(void)
{
    char topic[RPC_RESPONSE_TOPIC_MIN_LEN];

    uint8_t written = formatRpcResponseTopic(42, topic, sizeof(topic));

    TEST_ASSERT_EQUAL_STRING("v1/devices/me/rpc/response/42", topic);
    TEST_ASSERT_EQUAL_UINT8(strlen(topic), written);
}

static void test_largest_request_id_still_fits(void)
{
    char topic[RPC_RESPONSE_TOPIC_MIN_LEN];

    uint8_t written = formatRpcResponseTopic(4294967295u, topic, sizeof(topic));

    TEST_ASSERT_EQUAL_STRING("v1/devices/me/rpc/response/4294967295", topic);
    TEST_ASSERT_TRUE(written < RPC_RESPONSE_TOPIC_MIN_LEN);
}

static void test_reply_reports_the_led_state(void)
{
    char payload[RPC_RESPONSE_TEXT_MIN_LEN];

    formatRpcResponse(true, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_STRING("{\"ledOn\":true}", payload);

    formatRpcResponse(false, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_STRING("{\"ledOn\":false}", payload);
}

static void test_small_buffers_yield_nothing(void)
{
    char topic[RPC_RESPONSE_TOPIC_MIN_LEN];
    char payload[RPC_RESPONSE_TEXT_MIN_LEN];
    memset(topic, 'x', sizeof(topic));
    memset(payload, 'x', sizeof(payload));

    TEST_ASSERT_EQUAL_UINT8(0, formatRpcResponseTopic(42, topic,
                                                      RPC_RESPONSE_TOPIC_MIN_LEN - 1));
    TEST_ASSERT_EQUAL_UINT8(0, formatRpcResponse(true, payload,
                                                 RPC_RESPONSE_TEXT_MIN_LEN - 1));
    TEST_ASSERT_EQUAL_STRING("", topic);
    TEST_ASSERT_EQUAL_STRING("", payload);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_reply_topic_carries_the_request_id);
    RUN_TEST(test_largest_request_id_still_fits);
    RUN_TEST(test_reply_reports_the_led_state);
    RUN_TEST(test_small_buffers_yield_nothing);
    return UNITY_END();
}
