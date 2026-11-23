#include <unity.h>

#include "frameCodec.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t SAMPLE_PAYLOAD[] = { 0x01, 0x02, 0x7F, 0xFF };
static constexpr uint8_t SAMPLE_PAYLOAD_LEN = 4;

static uint8_t buildSampleFrame(uint8_t *frameOut)
{
    return encodeFrame(SAMPLE_PAYLOAD, SAMPLE_PAYLOAD_LEN, frameOut);
}

static void test_encoded_frame_has_the_expected_envelope(void)
{
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);

    TEST_ASSERT_EQUAL_UINT8(SAMPLE_PAYLOAD_LEN + FRAME_OVERHEAD, frameLen);
    TEST_ASSERT_EQUAL_HEX8(FRAME_HEAD, frame[FRAME_HEAD_OFFSET]);
    TEST_ASSERT_EQUAL_UINT8(SAMPLE_PAYLOAD_LEN, frame[FRAME_LENGTH_OFFSET]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(SAMPLE_PAYLOAD, frame + FRAME_PAYLOAD_OFFSET,
                                  SAMPLE_PAYLOAD_LEN);
}

static void test_roundtrip_returns_the_same_payload(void)
{
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = 0;
    TEST_ASSERT_EQUAL(FRAME_OK, decodeFrame(frame, frameLen, payload, &payloadLen));
    TEST_ASSERT_EQUAL_UINT8(SAMPLE_PAYLOAD_LEN, payloadLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(SAMPLE_PAYLOAD, payload, payloadLen);
}

// Un cadru fara PAYLOAD este raspunsul sclavului la o comanda necunoscuta,
// deci trebuie sa fie un cadru valid, nu un caz special.
static void test_empty_payload_is_a_valid_frame(void)
{
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t frameLen = encodeFrame(SAMPLE_PAYLOAD, 0, frame);
    TEST_ASSERT_EQUAL_UINT8(FRAME_OVERHEAD, frameLen);

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = 0xAA;
    TEST_ASSERT_EQUAL(FRAME_OK, decodeFrame(frame, frameLen, payload, &payloadLen));
    TEST_ASSERT_EQUAL_UINT8(0, payloadLen);
}

static void test_payload_over_the_limit_is_refused(void)
{
    uint8_t oversized[FRAME_MAX_PAYLOAD + 1] = { 0 };
    uint8_t frame[FRAME_MAX_LEN + 1];
    frame[FRAME_HEAD_OFFSET] = 0;

    TEST_ASSERT_EQUAL_UINT8(0, encodeFrame(oversized, FRAME_MAX_PAYLOAD + 1, frame));
    TEST_ASSERT_EQUAL_HEX8(0, frame[FRAME_HEAD_OFFSET]);   // nu s-a scris nimic
}

static void test_wrong_head_is_rejected(void)
{
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);
    frame[FRAME_HEAD_OFFSET] = 0x5A;

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = 0;
    TEST_ASSERT_EQUAL(FRAME_BAD_HEAD, decodeFrame(frame, frameLen, payload, &payloadLen));
}

static void test_length_over_the_limit_is_rejected(void)
{
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);
    frame[FRAME_LENGTH_OFFSET] = FRAME_MAX_PAYLOAD + 1;

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = 0;
    TEST_ASSERT_EQUAL(FRAME_BAD_LENGTH, decodeFrame(frame, frameLen, payload, &payloadLen));
}

static void test_flipped_payload_bit_is_caught(void)
{
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);
    frame[FRAME_PAYLOAD_OFFSET] ^= 0x01;

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = 0;
    TEST_ASSERT_EQUAL(FRAME_BAD_CHECKSUM, decodeFrame(frame, frameLen, payload, &payloadLen));
}

// Regresie: suma de control acopera si octetul LENGTH. Un LENGTH scurtat cu
// unu lasa PAYLOAD-ul si suma neatinse, deci numai includerea lui in suma
// prinde cazul.
static void test_shortened_length_is_caught_by_the_checksum(void)
{
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);
    frame[FRAME_LENGTH_OFFSET] = SAMPLE_PAYLOAD_LEN - 1;

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = 0;
    TEST_ASSERT_EQUAL(FRAME_BAD_CHECKSUM, decodeFrame(frame, frameLen, payload, &payloadLen));
}

static void test_truncated_answers_are_rejected(void)
{
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t frameLen = buildSampleFrame(frame);

    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint8_t payloadLen = 0;
    TEST_ASSERT_EQUAL(FRAME_TRUNCATED,
                      decodeFrame(frame, (uint8_t)(frameLen - 1), payload, &payloadLen));
    TEST_ASSERT_EQUAL(FRAME_TRUNCATED, decodeFrame(frame, 0, payload, &payloadLen));
    TEST_ASSERT_EQUAL(FRAME_TRUNCATED,
                      decodeFrame(frame, FRAME_OVERHEAD - 1, payload, &payloadLen));
}

static void test_checksum_covers_length_and_payload(void)
{
    uint8_t frame[FRAME_MAX_LEN];
    buildSampleFrame(frame);

    uint8_t expected = SAMPLE_PAYLOAD_LEN;
    for (uint8_t i = 0; i < SAMPLE_PAYLOAD_LEN; i++) expected ^= SAMPLE_PAYLOAD[i];

    TEST_ASSERT_EQUAL_HEX8(expected, frameChecksum(frame, SAMPLE_PAYLOAD_LEN));
    TEST_ASSERT_EQUAL_HEX8(expected, frame[FRAME_PAYLOAD_OFFSET + SAMPLE_PAYLOAD_LEN]);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_encoded_frame_has_the_expected_envelope);
    RUN_TEST(test_roundtrip_returns_the_same_payload);
    RUN_TEST(test_empty_payload_is_a_valid_frame);
    RUN_TEST(test_payload_over_the_limit_is_refused);
    RUN_TEST(test_wrong_head_is_rejected);
    RUN_TEST(test_length_over_the_limit_is_rejected);
    RUN_TEST(test_flipped_payload_bit_is_caught);
    RUN_TEST(test_shortened_length_is_caught_by_the_checksum);
    RUN_TEST(test_truncated_answers_are_rejected);
    RUN_TEST(test_checksum_covers_length_and_payload);
    return UNITY_END();
}
