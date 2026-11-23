#include <unity.h>

#include "distancePayload.h"

void setUp(void) {}
void tearDown(void) {}

// Un raport cu un senzor bun si unul fara ecou: cazul pe care masterul trebuie
// sa il afiseze corect, nu doar cel in care totul merge.
static DistanceReport buildMixedReport()
{
    DistanceReport report = { 42, SENSOR_COUNT, {} };
    report.readings[0] = { READING_OK, 1234 };
    report.readings[1] = { READING_TIMEOUT, 0 };
    return report;
}

static void test_packed_payload_has_the_protocol_length(void)
{
    DistanceReport report = buildMixedReport();
    uint8_t payload[FRAME_MAX_PAYLOAD];

    TEST_ASSERT_EQUAL_UINT8(DISTANCE_PAYLOAD_LEN, packDistancePayload(&report, payload));
}

static void test_roundtrip_preserves_sequence_and_readings(void)
{
    DistanceReport sent = buildMixedReport();
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&sent, payload);

    DistanceReport received = {};
    TEST_ASSERT_TRUE(unpackDistancePayload(payload, payloadLen, &received));
    TEST_ASSERT_EQUAL_UINT8(sent.sequence, received.sequence);
    TEST_ASSERT_EQUAL_UINT8(sent.sensorCount, received.sensorCount);
    TEST_ASSERT_EQUAL(READING_OK, received.readings[0].status);
    TEST_ASSERT_EQUAL_UINT16(1234, received.readings[0].distanceMm);
    TEST_ASSERT_EQUAL(READING_TIMEOUT, received.readings[1].status);
    TEST_ASSERT_EQUAL_UINT16(0, received.readings[1].distanceMm);
}

// Ordinea octetilor este parte din protocol: o schimbare aici strica masterul
// fara sa strice sclavul, deci se verifica explicit.
static void test_distance_is_little_endian(void)
{
    DistanceReport report = { 1, SENSOR_COUNT, {} };
    report.readings[0] = { READING_OK, 0x0102 };
    report.readings[1] = { READING_OK, 0x0304 };

    uint8_t payload[FRAME_MAX_PAYLOAD];
    packDistancePayload(&report, payload);

    TEST_ASSERT_EQUAL_HEX8(0x02, payload[PAYLOAD_HEADER_BYTES + 1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, payload[PAYLOAD_HEADER_BYTES + 2]);
    TEST_ASSERT_EQUAL_HEX8(0x04, payload[PAYLOAD_HEADER_BYTES + BYTES_PER_READING + 1]);
    TEST_ASSERT_EQUAL_HEX8(0x03, payload[PAYLOAD_HEADER_BYTES + BYTES_PER_READING + 2]);
}

static void test_sequence_wraps_without_losing_readings(void)
{
    DistanceReport report = buildMixedReport();
    report.sequence = 0xFF;

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&report, payload);

    DistanceReport received = {};
    TEST_ASSERT_TRUE(unpackDistancePayload(payload, payloadLen, &received));
    TEST_ASSERT_EQUAL_UINT8(0xFF, received.sequence);
    TEST_ASSERT_EQUAL_UINT16(1234, received.readings[0].distanceMm);
}

// Starea de la pornire trebuie sa traverseze magistrala ca atare: altfel
// masterul ar afisa 0 mm pentru un senzor care nu a fost citit niciodata.
static void test_not_yet_measured_survives_the_roundtrip(void)
{
    DistanceReport report = { 0, SENSOR_COUNT, {} };
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&report, payload);

    DistanceReport received = {};
    TEST_ASSERT_TRUE(unpackDistancePayload(payload, payloadLen, &received));
    TEST_ASSERT_EQUAL(READING_NOT_YET_MEASURED, received.readings[0].status);
    TEST_ASSERT_EQUAL(READING_NOT_YET_MEASURED, received.readings[1].status);
}

static void test_too_many_sensors_is_refused(void)
{
    DistanceReport report = buildMixedReport();
    report.sensorCount = SENSOR_COUNT + 1;

    uint8_t payload[FRAME_MAX_PAYLOAD];
    TEST_ASSERT_EQUAL_UINT8(0, packDistancePayload(&report, payload));
}

static void test_short_payload_is_refused(void)
{
    DistanceReport report = buildMixedReport();
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&report, payload);

    DistanceReport received = {};
    TEST_ASSERT_FALSE(unpackDistancePayload(payload, (uint8_t)(payloadLen - 1), &received));
    TEST_ASSERT_FALSE(unpackDistancePayload(payload, 1, &received));
    TEST_ASSERT_FALSE(unpackDistancePayload(payload, 0, &received));
}

// PAYLOAD-ul vine de la alt microcontroler, deci un numar de senzori sau o
// stare care nu exista in protocol trebuie respinse, nu convertite la enum.
static void test_corrupt_fields_are_refused(void)
{
    DistanceReport report = buildMixedReport();
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&report, payload);
    DistanceReport received = {};

    payload[1] = SENSOR_COUNT + 1;
    TEST_ASSERT_FALSE(unpackDistancePayload(payload, payloadLen, &received));

    payload[1] = SENSOR_COUNT;
    payload[PAYLOAD_HEADER_BYTES] = 0x7E;
    TEST_ASSERT_FALSE(unpackDistancePayload(payload, payloadLen, &received));
}

// Sclavul poate raporta mai putini senzori decat incap in cadru, iar masterul
// trebuie sa accepte raspunsul fara recompilare.
static void test_fewer_sensors_than_the_maximum_are_accepted(void)
{
    DistanceReport report = { 7, 1, {} };
    report.readings[0] = { READING_OK, 500 };

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&report, payload);
    TEST_ASSERT_EQUAL_UINT8(PAYLOAD_HEADER_BYTES + BYTES_PER_READING, payloadLen);

    DistanceReport received = {};
    TEST_ASSERT_TRUE(unpackDistancePayload(payload, payloadLen, &received));
    TEST_ASSERT_EQUAL_UINT8(1, received.sensorCount);
    TEST_ASSERT_EQUAL_UINT16(500, received.readings[0].distanceMm);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_packed_payload_has_the_protocol_length);
    RUN_TEST(test_roundtrip_preserves_sequence_and_readings);
    RUN_TEST(test_distance_is_little_endian);
    RUN_TEST(test_sequence_wraps_without_losing_readings);
    RUN_TEST(test_not_yet_measured_survives_the_roundtrip);
    RUN_TEST(test_too_many_sensors_is_refused);
    RUN_TEST(test_short_payload_is_refused);
    RUN_TEST(test_corrupt_fields_are_refused);
    RUN_TEST(test_fewer_sensors_than_the_maximum_are_accepted);
    return UNITY_END();
}
