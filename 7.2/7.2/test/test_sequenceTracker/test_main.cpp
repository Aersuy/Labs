#include <unity.h>

#include "sequenceTracker.h"

void setUp(void) {}
void tearDown(void) {}

static SequenceTracker tracker;
static uint8_t lostCount;

static SequenceStatus accept(uint8_t sequence)
{
    return sequenceTrackerAccept(&tracker, sequence, &lostCount);
}

static void startTracker(uint8_t firstSequence)
{
    sequenceTrackerReset(&tracker);
    TEST_ASSERT_EQUAL(SEQUENCE_FIRST, accept(firstSequence));
}

static void test_first_packet_is_not_a_gap(void)
{
    sequenceTrackerReset(&tracker);

    TEST_ASSERT_EQUAL(SEQUENCE_FIRST, accept(200));
    TEST_ASSERT_EQUAL_UINT8(0, lostCount);
}

static void test_consecutive_packets_are_in_order(void)
{
    startTracker(10);

    TEST_ASSERT_EQUAL(SEQUENCE_IN_ORDER, accept(11));
    TEST_ASSERT_EQUAL(SEQUENCE_IN_ORDER, accept(12));
    TEST_ASSERT_EQUAL_UINT8(0, lostCount);
}

static void test_a_jump_reports_how_many_packets_are_missing(void)
{
    startTracker(10);

    TEST_ASSERT_EQUAL(SEQUENCE_GAP, accept(14));
    TEST_ASSERT_EQUAL_UINT8(3, lostCount);
}

// Dupa un salt, urmarirea continua de la pachetul primit, nu de la cel pierdut:
// altfel fiecare pachet urmator ar fi raportat tot ca gaura.
static void test_tracking_continues_after_a_gap(void)
{
    startTracker(10);
    TEST_ASSERT_EQUAL(SEQUENCE_GAP, accept(14));

    TEST_ASSERT_EQUAL(SEQUENCE_IN_ORDER, accept(15));
    TEST_ASSERT_EQUAL_UINT8(0, lostCount);
}

// Retransmisia masterului pastreaza acelasi SEQ: trebuie recunoscuta ca dublura
// si nu are voie sa strice numaratoarea.
static void test_repeated_sequence_is_a_duplicate(void)
{
    startTracker(10);
    TEST_ASSERT_EQUAL(SEQUENCE_IN_ORDER, accept(11));

    TEST_ASSERT_EQUAL(SEQUENCE_DUPLICATE, accept(11));
    TEST_ASSERT_EQUAL_UINT8(0, lostCount);
    TEST_ASSERT_EQUAL(SEQUENCE_IN_ORDER, accept(12));
}

// Un pachet ramas in urma (ecou intarziat) nu este un salt de 250 de pachete.
static void test_older_sequence_is_a_duplicate_not_a_gap(void)
{
    startTracker(10);

    TEST_ASSERT_EQUAL(SEQUENCE_DUPLICATE, accept(5));
    TEST_ASSERT_EQUAL(SEQUENCE_IN_ORDER, accept(11));
}

static void test_sequence_wraps_from_255_to_zero(void)
{
    startTracker(255);

    TEST_ASSERT_EQUAL(SEQUENCE_IN_ORDER, accept(0));
    TEST_ASSERT_EQUAL(SEQUENCE_GAP, accept(3));
    TEST_ASSERT_EQUAL_UINT8(2, lostCount);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_packet_is_not_a_gap);
    RUN_TEST(test_consecutive_packets_are_in_order);
    RUN_TEST(test_a_jump_reports_how_many_packets_are_missing);
    RUN_TEST(test_tracking_continues_after_a_gap);
    RUN_TEST(test_repeated_sequence_is_a_duplicate);
    RUN_TEST(test_older_sequence_is_a_duplicate_not_a_gap);
    RUN_TEST(test_sequence_wraps_from_255_to_zero);
    return UNITY_END();
}
