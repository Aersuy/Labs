#include <unity.h>
#include <string.h>

#include "trafficFsm.h"

void setUp(void) {}
void tearDown(void) {}

static bool isGreen(LampColor color) { return color == LAMP_GREEN; }

// Cerinta de siguranta a intersectiei: nicio stare nu da verde pe ambele
// directii in acelasi timp.
static void test_no_state_shows_two_greens(void)
{
    for (uint8_t state = 0; state < TRAFFIC_STATE_COUNT; state++)
    {
        SignalPattern pattern = trafficFsmPattern((TrafficState)state);
        TEST_ASSERT_FALSE(isGreen(pattern.east) && isGreen(pattern.north));
    }
}

// Est are prioritate: fara cerere, verdele de pe Est doar trece din faza
// minima in faza nelimitata si ramane acolo.
static void test_east_keeps_the_green_without_a_request(void)
{
    TEST_ASSERT_EQUAL(EAST_GREEN_HOLD,
                      trafficFsmNext(EAST_GREEN_MIN, EVENT_PHASE_TIMEOUT));
    TEST_ASSERT_EQUAL(LAMP_GREEN, trafficFsmPattern(EAST_GREEN_HOLD).east);
    TEST_ASSERT_EQUAL(LAMP_RED, trafficFsmPattern(EAST_GREEN_HOLD).north);
}

// Verdele minim nu poate fi scurtat de o apasare.
static void test_request_does_not_cut_the_minimum_green(void)
{
    TEST_ASSERT_FALSE(trafficFsmServesRequest(EAST_GREEN_MIN));
    TEST_ASSERT_EQUAL(EAST_GREEN_MIN,
                      trafficFsmNext(EAST_GREEN_MIN, EVENT_NORTH_REQUEST));
}

static void test_only_the_hold_state_serves_a_request(void)
{
    TEST_ASSERT_TRUE(trafficFsmServesRequest(EAST_GREEN_HOLD));

    for (uint8_t state = 0; state < TRAFFIC_STATE_COUNT; state++)
    {
        if ((TrafficState)state == EAST_GREEN_HOLD) continue;
        TEST_ASSERT_FALSE(trafficFsmServesRequest((TrafficState)state));
    }
}

// Ciclul complet cerut de enunt, parcurs eveniment cu eveniment.
static void test_request_runs_the_full_cycle_back_to_east(void)
{
    TEST_ASSERT_EQUAL(EAST_YELLOW,
                      trafficFsmNext(EAST_GREEN_HOLD, EVENT_NORTH_REQUEST));
    TEST_ASSERT_EQUAL(ALL_RED_BEFORE_NORTH,
                      trafficFsmNext(EAST_YELLOW, EVENT_PHASE_TIMEOUT));
    TEST_ASSERT_EQUAL(NORTH_GREEN,
                      trafficFsmNext(ALL_RED_BEFORE_NORTH, EVENT_PHASE_TIMEOUT));
    TEST_ASSERT_EQUAL(NORTH_YELLOW,
                      trafficFsmNext(NORTH_GREEN, EVENT_PHASE_TIMEOUT));
    TEST_ASSERT_EQUAL(ALL_RED_BEFORE_EAST,
                      trafficFsmNext(NORTH_YELLOW, EVENT_PHASE_TIMEOUT));
    TEST_ASSERT_EQUAL(EAST_GREEN_MIN,
                      trafficFsmNext(ALL_RED_BEFORE_EAST, EVENT_PHASE_TIMEOUT));
}

// Regula de semnalizare: verde -> galben -> rosu. Un verde nu poate deveni
// rosu fara galben pe aceeasi directie.
static void test_green_turns_yellow_before_red(void)
{
    for (uint8_t state = 0; state < TRAFFIC_STATE_COUNT; state++)
    {
        SignalPattern before = trafficFsmPattern((TrafficState)state);
        TrafficEvent leaving = trafficFsmServesRequest((TrafficState)state)
                                  ? EVENT_NORTH_REQUEST
                                  : EVENT_PHASE_TIMEOUT;
        SignalPattern after =
            trafficFsmPattern(trafficFsmNext((TrafficState)state, leaving));

        if (isGreen(before.east))  TEST_ASSERT_NOT_EQUAL(LAMP_RED, after.east);
        if (isGreen(before.north)) TEST_ASSERT_NOT_EQUAL(LAMP_RED, after.north);
    }
}

// Degajarea intersectiei: intre doua verzi trece o faza cu ambele rosii.
static void test_clearance_states_show_red_on_both_directions(void)
{
    const TrafficState clearances[] = { ALL_RED_BEFORE_NORTH, ALL_RED_BEFORE_EAST };

    for (uint8_t i = 0; i < 2; i++)
    {
        SignalPattern pattern = trafficFsmPattern(clearances[i]);
        TEST_ASSERT_EQUAL(LAMP_RED, pattern.east);
        TEST_ASSERT_EQUAL(LAMP_RED, pattern.north);
    }
}

static void test_night_switch_works_from_every_state(void)
{
    for (uint8_t state = 0; state < TRAFFIC_STATE_COUNT; state++)
    {
        if ((TrafficState)state == NIGHT_FLASH) continue;
        TEST_ASSERT_EQUAL(NIGHT_FLASH,
                          trafficFsmNext((TrafficState)state, EVENT_NIGHT_TOGGLE));
    }
}

// Intersectia inactiva intra singura in mod noapte, din starea de asteptare.
static void test_idle_hold_falls_into_night_mode(void)
{
    TEST_ASSERT_EQUAL(NIGHT_FLASH,
                      trafficFsmNext(EAST_GREEN_HOLD, EVENT_PHASE_TIMEOUT));
}

static void test_night_mode_flashes_yellow_on_both_directions(void)
{
    SignalPattern pattern = trafficFsmPattern(NIGHT_FLASH);

    TEST_ASSERT_EQUAL(LAMP_YELLOW, pattern.east);
    TEST_ASSERT_EQUAL(LAMP_YELLOW, pattern.north);
    TEST_ASSERT_EQUAL_UINT16(NIGHT_FLASH_PERIOD_MS / 2, pattern.blinkHalfPeriodMs);
}

// Galbenul intermitent nu expira singur: iese la comutator sau la primul
// vehicul detectat.
static void test_night_mode_is_left_only_by_an_event(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, trafficFsmPhaseMs(NIGHT_FLASH));
    TEST_ASSERT_EQUAL(NIGHT_FLASH, trafficFsmNext(NIGHT_FLASH, EVENT_PHASE_TIMEOUT));
    TEST_ASSERT_EQUAL(EAST_GREEN_MIN, trafficFsmNext(NIGHT_FLASH, EVENT_NIGHT_TOGGLE));
    TEST_ASSERT_EQUAL(EAST_GREEN_MIN, trafficFsmNext(NIGHT_FLASH, EVENT_NORTH_REQUEST));
}

// Doar modul noapte asteapta la infinit; orice alta faza are durata proprie,
// altfel automatul s-ar bloca intr-o faza de tranzit.
static void test_every_state_but_night_has_a_duration(void)
{
    for (uint8_t state = 0; state < TRAFFIC_STATE_COUNT; state++)
    {
        if ((TrafficState)state == NIGHT_FLASH) continue;
        TEST_ASSERT_GREATER_THAN_UINT16(0, trafficFsmPhaseMs((TrafficState)state));
    }
}

// Ierarhia duratelor: degajarea este cea mai scurta faza, iar asteptarea
// pana la modul noapte cea mai lunga.
static void test_phase_durations_keep_their_order(void)
{
    TEST_ASSERT_GREATER_THAN_UINT16(ALL_RED_MS, YELLOW_MS);
    TEST_ASSERT_GREATER_THAN_UINT16(YELLOW_MS, NORTH_GREEN_MS);
    TEST_ASSERT_GREATER_THAN_UINT16(EAST_MIN_GREEN_MS, EAST_HOLD_IDLE_MS);
}

// Automatul este complet: nicio combinatie nu cade intr-o stare inexistenta.
static void test_every_transition_lands_on_a_valid_state(void)
{
    for (uint8_t state = 0; state < TRAFFIC_STATE_COUNT; state++)
    {
        for (uint8_t event = 0; event < TRAFFIC_EVENT_COUNT; event++)
        {
            TrafficState next =
                trafficFsmNext((TrafficState)state, (TrafficEvent)event);
            TEST_ASSERT_LESS_THAN_UINT8(TRAFFIC_STATE_COUNT, (uint8_t)next);
        }
    }
}

// Jurnalul serial este cerinta de lucrare, deci starile trebuie sa fie
// deosebite una de alta in text, nu doar in cod.
static void test_state_names_are_distinct(void)
{
    for (uint8_t i = 0; i < TRAFFIC_STATE_COUNT; i++)
    {
        const char *name = trafficFsmStateName((TrafficState)i);
        TEST_ASSERT_TRUE(strlen(name) > 0);

        for (uint8_t j = i + 1; j < TRAFFIC_STATE_COUNT; j++)
        {
            TEST_ASSERT_TRUE(strcmp(name, trafficFsmStateName((TrafficState)j)) != 0);
        }
    }
}

static void test_lamp_colors_have_distinct_names(void)
{
    const LampColor colors[] = { LAMP_OFF, LAMP_RED, LAMP_YELLOW, LAMP_GREEN };
    const uint8_t count = 4;

    for (uint8_t i = 0; i < count; i++)
    {
        for (uint8_t j = i + 1; j < count; j++)
        {
            TEST_ASSERT_TRUE(
                strcmp(lampColorName(colors[i]), lampColorName(colors[j])) != 0);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_state_shows_two_greens);
    RUN_TEST(test_east_keeps_the_green_without_a_request);
    RUN_TEST(test_request_does_not_cut_the_minimum_green);
    RUN_TEST(test_only_the_hold_state_serves_a_request);
    RUN_TEST(test_request_runs_the_full_cycle_back_to_east);
    RUN_TEST(test_green_turns_yellow_before_red);
    RUN_TEST(test_clearance_states_show_red_on_both_directions);
    RUN_TEST(test_night_switch_works_from_every_state);
    RUN_TEST(test_idle_hold_falls_into_night_mode);
    RUN_TEST(test_night_mode_flashes_yellow_on_both_directions);
    RUN_TEST(test_night_mode_is_left_only_by_an_event);
    RUN_TEST(test_every_state_but_night_has_a_duration);
    RUN_TEST(test_phase_durations_keep_their_order);
    RUN_TEST(test_every_transition_lands_on_a_valid_state);
    RUN_TEST(test_state_names_are_distinct);
    RUN_TEST(test_lamp_colors_have_distinct_names);
    return UNITY_END();
}
