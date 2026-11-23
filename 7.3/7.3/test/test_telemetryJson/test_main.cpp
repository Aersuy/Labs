#include <unity.h>
#include <string.h>

#include "telemetryJson.h"

void setUp(void) {}
void tearDown(void) {}

static void test_telemetry_carries_temperature_and_led_state(void)
{
    char payload[TELEMETRY_TEXT_MIN_LEN];

    uint8_t written = formatTelemetry(234, true, payload, sizeof(payload));

    TEST_ASSERT_EQUAL_STRING("{\"temperature\":23.4,\"ledOn\":true}", payload);
    TEST_ASSERT_EQUAL_UINT8(strlen(payload), written);
}

static void test_stopped_led_is_reported_as_false(void)
{
    char payload[TELEMETRY_TEXT_MIN_LEN];

    formatTelemetry(0, false, payload, sizeof(payload));

    TEST_ASSERT_EQUAL_STRING("{\"temperature\":0.0,\"ledOn\":false}", payload);
}

// Senzorul nelegat citeste 0 pe ADC, ceea ce da circa -273 C: valoarea trebuie
// sa ajunga pe tabloul de bord ca numar, nu stricata.
static void test_negative_temperature_keeps_its_sign(void)
{
    char payload[TELEMETRY_TEXT_MIN_LEN];

    formatTelemetry(-2731, false, payload, sizeof(payload));

    TEST_ASSERT_EQUAL_STRING("{\"temperature\":-273.1,\"ledOn\":false}", payload);
}

static void test_extreme_value_still_fits_the_minimum_buffer(void)
{
    char payload[TELEMETRY_TEXT_MIN_LEN];

    uint8_t written = formatTelemetry(-32767, false, payload, sizeof(payload));

    TEST_ASSERT_EQUAL_STRING("{\"temperature\":-3276.7,\"ledOn\":false}", payload);
    TEST_ASSERT_TRUE(written < TELEMETRY_TEXT_MIN_LEN);
}

// Un tampon prea mic nu primeste JSON trunchiat: ar fi un mesaj care pare
// valid si pe care serverul l-ar refuza fara explicatie.
static void test_small_buffer_yields_nothing(void)
{
    char payload[TELEMETRY_TEXT_MIN_LEN];
    memset(payload, 'x', sizeof(payload));

    uint8_t written = formatTelemetry(234, true, payload, TELEMETRY_TEXT_MIN_LEN - 1);

    TEST_ASSERT_EQUAL_UINT8(0, written);
    TEST_ASSERT_EQUAL_STRING("", payload);
}

static void test_missing_buffer_is_refused(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, formatTelemetry(234, true, NULL, TELEMETRY_TEXT_MIN_LEN));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_telemetry_carries_temperature_and_led_state);
    RUN_TEST(test_stopped_led_is_reported_as_false);
    RUN_TEST(test_negative_temperature_keeps_its_sign);
    RUN_TEST(test_extreme_value_still_fits_the_minimum_buffer);
    RUN_TEST(test_small_buffer_yields_nothing);
    RUN_TEST(test_missing_buffer_is_refused);
    return UNITY_END();
}
