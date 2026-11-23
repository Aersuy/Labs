#include <unity.h>
#include <string.h>

#include "rpcCommand.h"

void setUp(void) {}
void tearDown(void) {}

static const char VALID_TOPIC[] = "v1/devices/me/rpc/request/42";

static RpcParseResult parse(const char *topic, const char *json, RpcRequest *requestOut)
{
    return parseRpcRequest(topic, json, (uint16_t)strlen(json), requestOut);
}

// --- cazul normal -----------------------------------------------------------

static void test_switch_on_is_accepted(void)
{
    RpcRequest request;

    RpcParseResult result = parse(VALID_TOPIC, "{\"method\":\"setLedState\",\"params\":true}",
                                  &request);

    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_OK, result);
    TEST_ASSERT_EQUAL_UINT8(RPC_SET_LED, request.method);
    TEST_ASSERT_TRUE(request.isOn);
    TEST_ASSERT_EQUAL_UINT32(42, request.requestId);
}

static void test_switch_off_is_accepted(void)
{
    RpcRequest request;

    parse(VALID_TOPIC, "{\"method\":\"setLedState\",\"params\":false}", &request);

    TEST_ASSERT_FALSE(request.isOn);
}

// Unele configuratii de widget ambaleaza starea, iar incercarile manuale
// trimit 0/1: toate trei spun acelasi lucru.
static void test_wrapped_and_numeric_parameters_mean_the_same(void)
{
    RpcRequest wrapped;
    RpcRequest numeric;

    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_OK,
        parse(VALID_TOPIC, "{\"method\":\"setLedState\",\"params\":{\"value\":true}}", &wrapped));
    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_OK,
        parse(VALID_TOPIC, "{\"method\":\"setLedState\",\"params\":1}", &numeric));

    TEST_ASSERT_TRUE(wrapped.isOn);
    TEST_ASSERT_TRUE(numeric.isOn);
}

static void test_state_query_needs_no_parameter(void)
{
    RpcRequest request;

    RpcParseResult result = parse(VALID_TOPIC, "{\"method\":\"getLedState\"}", &request);

    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_OK, result);
    TEST_ASSERT_EQUAL_UINT8(RPC_GET_LED, request.method);
}

static void test_large_request_id_survives(void)
{
    RpcRequest request;

    parse("v1/devices/me/rpc/request/4294967295", "{\"method\":\"getLedState\"}", &request);

    TEST_ASSERT_EQUAL_UINT32(4294967295u, request.requestId);
}

// --- subiecte refuzate ------------------------------------------------------

static void test_foreign_topic_is_refused(void)
{
    RpcRequest request;

    RpcParseResult result = parse("v1/devices/me/attributes", "{\"method\":\"getLedState\"}",
                                  &request);

    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_TOPIC, result);
}

static void test_topic_without_a_number_is_refused(void)
{
    RpcRequest request;
    const char *json = "{\"method\":\"getLedState\"}";

    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_TOPIC,
        parse("v1/devices/me/rpc/request/", json, &request));
    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_TOPIC,
        parse("v1/devices/me/rpc/request/12a", json, &request));
    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_TOPIC,
        parse("v1/devices/me/rpc/request/99999999999", json, &request));
}

// --- corpuri refuzate -------------------------------------------------------

static void test_malformed_json_is_refused(void)
{
    RpcRequest request;

    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_JSON,
        parse(VALID_TOPIC, "{\"method\":", &request));
    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_JSON,
        parse(VALID_TOPIC, "[1,2,3]", &request));
    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_JSON,
        parseRpcRequest(VALID_TOPIC, "{}", 0, &request));
}

static void test_unknown_method_is_refused(void)
{
    RpcRequest request;

    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_UNKNOWN_METHOD,
        parse(VALID_TOPIC, "{\"method\":\"reboot\",\"params\":true}", &request));
    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_UNKNOWN_METHOD,
        parse(VALID_TOPIC, "{\"params\":true}", &request));
    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_UNKNOWN_METHOD,
        parse(VALID_TOPIC, "{\"method\":7}", &request));
}

static void test_parameter_that_is_not_a_state_is_refused(void)
{
    RpcRequest request;

    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_PARAMS,
        parse(VALID_TOPIC, "{\"method\":\"setLedState\"}", &request));
    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_PARAMS,
        parse(VALID_TOPIC, "{\"method\":\"setLedState\",\"params\":\"true\"}", &request));
    TEST_ASSERT_EQUAL_UINT8(RPC_PARSE_BAD_PARAMS,
        parse(VALID_TOPIC, "{\"method\":\"setLedState\",\"params\":5}", &request));
}

// Contractul este ca requestOut se scrie numai la RPC_PARSE_OK: altfel un apel
// refuzat ar lasa in urma un id care pare valid.
static void test_refused_request_is_not_written(void)
{
    RpcRequest request;
    request.requestId = 7;
    request.method = RPC_GET_LED;
    request.isOn = true;

    parse(VALID_TOPIC, "{\"method\":\"setLedState\",\"params\":\"nu\"}", &request);

    TEST_ASSERT_EQUAL_UINT32(7, request.requestId);
    TEST_ASSERT_TRUE(request.isOn);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_switch_on_is_accepted);
    RUN_TEST(test_switch_off_is_accepted);
    RUN_TEST(test_wrapped_and_numeric_parameters_mean_the_same);
    RUN_TEST(test_state_query_needs_no_parameter);
    RUN_TEST(test_large_request_id_survives);
    RUN_TEST(test_foreign_topic_is_refused);
    RUN_TEST(test_topic_without_a_number_is_refused);
    RUN_TEST(test_malformed_json_is_refused);
    RUN_TEST(test_unknown_method_is_refused);
    RUN_TEST(test_parameter_that_is_not_a_state_is_refused);
    RUN_TEST(test_refused_request_is_not_written);
    return UNITY_END();
}
