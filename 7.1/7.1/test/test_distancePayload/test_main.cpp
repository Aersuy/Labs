#include <unity.h>

#include "distancePayload.h"

void setUp(void) {}
void tearDown(void) {}

static DistanceReport buildReport(uint8_t sequence, DistanceReading reading)
{
    DistanceReport report = { sequence, reading };
    return report;
}

static void test_packed_payload_has_the_protocol_length(void)
{
    DistanceReport report = buildReport(42, { READING_OK, 1234 });
    uint8_t payload[FRAME_MAX_PAYLOAD];

    TEST_ASSERT_EQUAL_UINT8(DISTANCE_PAYLOAD_LEN, packDistancePayload(&report, payload));
}

static void test_roundtrip_preserves_sequence_and_reading(void)
{
    DistanceReport sent = buildReport(42, { READING_OK, 1234 });
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&sent, payload);

    DistanceReport received = {};
    TEST_ASSERT_TRUE(unpackDistancePayload(payload, payloadLen, &received));
    TEST_ASSERT_EQUAL_UINT8(sent.sequence, received.sequence);
    TEST_ASSERT_EQUAL(READING_OK, received.reading.status);
    TEST_ASSERT_EQUAL_UINT16(1234, received.reading.distanceMm);
}

// Un senzor fara ecou nu trebuie sa ajunga la master ca "0 mm": starea
// traverseaza magistrala alaturi de valoare.
static void test_timeout_survives_the_roundtrip(void)
{
    DistanceReport sent = buildReport(7, { READING_TIMEOUT, 0 });
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&sent, payload);

    DistanceReport received = {};
    TEST_ASSERT_TRUE(unpackDistancePayload(payload, payloadLen, &received));
    TEST_ASSERT_EQUAL(READING_TIMEOUT, received.reading.status);
    TEST_ASSERT_EQUAL_UINT16(0, received.reading.distanceMm);
}

// Ordinea octetilor este parte din protocol: o schimbare aici strica masterul
// fara sa strice sclavul, deci se verifica explicit.
static void test_distance_is_little_endian(void)
{
    DistanceReport report = buildReport(1, { READING_OK, 0x0102 });

    uint8_t payload[FRAME_MAX_PAYLOAD];
    packDistancePayload(&report, payload);

    TEST_ASSERT_EQUAL_HEX8(READING_OK, payload[PAYLOAD_HEADER_BYTES]);
    TEST_ASSERT_EQUAL_HEX8(0x02, payload[PAYLOAD_HEADER_BYTES + 1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, payload[PAYLOAD_HEADER_BYTES + 2]);
}

static void test_sequence_wraps_without_losing_the_reading(void)
{
    DistanceReport report = buildReport(0xFF, { READING_OK, 1234 });

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&report, payload);

    DistanceReport received = {};
    TEST_ASSERT_TRUE(unpackDistancePayload(payload, payloadLen, &received));
    TEST_ASSERT_EQUAL_UINT8(0xFF, received.sequence);
    TEST_ASSERT_EQUAL_UINT16(1234, received.reading.distanceMm);
}

// Starea de la pornire trebuie sa traverseze magistrala ca atare: altfel
// masterul ar afisa 0 mm pentru un senzor care nu a fost citit niciodata.
static void test_not_yet_measured_survives_the_roundtrip(void)
{
    DistanceReport report = { 0, {} };
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&report, payload);

    DistanceReport received = {};
    TEST_ASSERT_TRUE(unpackDistancePayload(payload, payloadLen, &received));
    TEST_ASSERT_EQUAL(READING_NOT_YET_MEASURED, received.reading.status);
}

static void test_short_payload_is_refused(void)
{
    DistanceReport report = buildReport(42, { READING_OK, 1234 });
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&report, payload);

    DistanceReport received = {};
    TEST_ASSERT_FALSE(unpackDistancePayload(payload, (uint8_t)(payloadLen - 1), &received));
    TEST_ASSERT_FALSE(unpackDistancePayload(payload, 1, &received));
    TEST_ASSERT_FALSE(unpackDistancePayload(payload, 0, &received));
}

// PAYLOAD-ul vine de la alt microcontroler, deci o stare care nu exista in
// protocol trebuie respinsa, nu convertita la enum.
static void test_unknown_status_is_refused(void)
{
    DistanceReport report = buildReport(42, { READING_OK, 1234 });
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = packDistancePayload(&report, payload);

    payload[PAYLOAD_HEADER_BYTES] = 0x7E;

    DistanceReport received = {};
    TEST_ASSERT_FALSE(unpackDistancePayload(payload, payloadLen, &received));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_packed_payload_has_the_protocol_length);
    RUN_TEST(test_roundtrip_preserves_sequence_and_reading);
    RUN_TEST(test_timeout_survives_the_roundtrip);
    RUN_TEST(test_distance_is_little_endian);
    RUN_TEST(test_sequence_wraps_without_losing_the_reading);
    RUN_TEST(test_not_yet_measured_survives_the_roundtrip);
    RUN_TEST(test_short_payload_is_refused);
    RUN_TEST(test_unknown_status_is_refused);
    return UNITY_END();
}
