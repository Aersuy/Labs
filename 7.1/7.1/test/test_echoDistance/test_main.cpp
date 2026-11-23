#include <unity.h>
#include <string.h>

#include "echoDistance.h"

void setUp(void) {}
void tearDown(void) {}

// Valori calculate cu factorul 1715/10000 mm/us folosit de modul:
// 117 us -> 20 mm (limita de jos), 116 us -> 19 mm (sub limita),
// 5831 us -> 1000 mm, 23323 us -> 3999 mm, 23330 us -> 4001 mm (peste limita).
static constexpr uint32_t MICROS_AT_MIN_RANGE = 117;
static constexpr uint32_t MICROS_UNDER_MIN_RANGE = 116;
static constexpr uint32_t MICROS_AT_ONE_METER = 5831;
static constexpr uint32_t MICROS_NEAR_MAX_RANGE = 23323;
static constexpr uint32_t MICROS_OVER_MAX_RANGE = 23330;

static void test_no_echo_is_a_timeout(void)
{
    DistanceReading reading = echoMicrosToReading(0);

    TEST_ASSERT_EQUAL(READING_TIMEOUT, reading.status);
    TEST_ASSERT_EQUAL_UINT16(0, reading.distanceMm);
}

// Fereastra de asteptare este si limita superioara acceptata: pulseIn() poate
// intoarce exact valoarea timeout-ului cand ecoul vine prea tarziu.
static void test_echo_at_or_over_the_window_is_a_timeout(void)
{
    TEST_ASSERT_EQUAL(READING_TIMEOUT, echoMicrosToReading(ECHO_TIMEOUT_US).status);
    TEST_ASSERT_EQUAL(READING_TIMEOUT, echoMicrosToReading(ECHO_TIMEOUT_US + 1).status);
    TEST_ASSERT_EQUAL(READING_TIMEOUT, echoMicrosToReading(100000).status);
}

static void test_one_meter_converts_to_one_thousand_millimeters(void)
{
    DistanceReading reading = echoMicrosToReading(MICROS_AT_ONE_METER);

    TEST_ASSERT_EQUAL(READING_OK, reading.status);
    TEST_ASSERT_EQUAL_UINT16(1000, reading.distanceMm);
}

static void test_range_boundaries_are_accepted(void)
{
    DistanceReading nearReading = echoMicrosToReading(MICROS_AT_MIN_RANGE);
    TEST_ASSERT_EQUAL(READING_OK, nearReading.status);
    TEST_ASSERT_EQUAL_UINT16(HCSR04_MIN_MM, nearReading.distanceMm);

    DistanceReading farReading = echoMicrosToReading(MICROS_NEAR_MAX_RANGE);
    TEST_ASSERT_EQUAL(READING_OK, farReading.status);
    TEST_ASSERT_EQUAL_UINT16(HCSR04_MAX_MM - 1, farReading.distanceMm);
}

// In afara domeniului senzorului valoarea nu se raporteaza: ar fi o distanta
// in care nu se poate avea incredere, iar 0 mm ar fi tot o distanta.
static void test_outside_the_range_reports_no_distance(void)
{
    DistanceReading tooNear = echoMicrosToReading(MICROS_UNDER_MIN_RANGE);
    TEST_ASSERT_EQUAL(READING_OUT_OF_RANGE, tooNear.status);
    TEST_ASSERT_EQUAL_UINT16(0, tooNear.distanceMm);

    DistanceReading tooFar = echoMicrosToReading(MICROS_OVER_MAX_RANGE);
    TEST_ASSERT_EQUAL(READING_OUT_OF_RANGE, tooFar.status);
    TEST_ASSERT_EQUAL_UINT16(0, tooFar.distanceMm);
}

static void test_conversion_is_monotonic_over_the_window(void)
{
    uint16_t previousMm = 0;
    for (uint32_t micros = 200; micros < ECHO_TIMEOUT_US; micros += 500)
    {
        uint16_t currentMm = echoMicrosToMillimeters(micros);
        TEST_ASSERT_TRUE(currentMm >= previousMm);
        previousMm = currentMm;
    }
}

// Octetul de stare vine de pe magistrala, deci validarea lui este un contract
// public al modulului, nu un detaliu intern.
static void test_only_defined_statuses_are_known(void)
{
    TEST_ASSERT_TRUE(isKnownReadingStatus(READING_NOT_YET_MEASURED));
    TEST_ASSERT_TRUE(isKnownReadingStatus(READING_OK));
    TEST_ASSERT_TRUE(isKnownReadingStatus(READING_TIMEOUT));
    TEST_ASSERT_TRUE(isKnownReadingStatus(READING_OUT_OF_RANGE));

    TEST_ASSERT_FALSE(isKnownReadingStatus((uint8_t)READING_OUT_OF_RANGE + 1));
    TEST_ASSERT_FALSE(isKnownReadingStatus(0xFF));
}

static void test_status_names_are_distinct(void)
{
    const char *okName = readingStatusName(READING_OK);
    const char *timeoutName = readingStatusName(READING_TIMEOUT);
    const char *rangeName = readingStatusName(READING_OUT_OF_RANGE);

    TEST_ASSERT_NOT_EQUAL(0, strcmp(okName, timeoutName));
    TEST_ASSERT_NOT_EQUAL(0, strcmp(okName, rangeName));
    TEST_ASSERT_NOT_EQUAL(0, strcmp(timeoutName, rangeName));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_echo_is_a_timeout);
    RUN_TEST(test_echo_at_or_over_the_window_is_a_timeout);
    RUN_TEST(test_one_meter_converts_to_one_thousand_millimeters);
    RUN_TEST(test_range_boundaries_are_accepted);
    RUN_TEST(test_outside_the_range_reports_no_distance);
    RUN_TEST(test_conversion_is_monotonic_over_the_window);
    RUN_TEST(test_only_defined_statuses_are_known);
    RUN_TEST(test_status_names_are_distinct);
    return UNITY_END();
}
