#ifndef TRAFFIC_FSM_H
#define TRAFFIC_FSM_H

#include <stdint.h>

// Un singur automat pentru toata intersectia, nu unul per directie: asa
// invariantul "niciodata doua verzi" rezulta din tabelul de stari, nu din
// sincronizarea corecta a doua automate independente.
enum TrafficState : uint8_t {
    EAST_GREEN_MIN = 0,       // verde pe Est, durata minima garantata
    EAST_GREEN_HOLD,          // verde pe Est pe termen nelimitat (prioritate)
    EAST_YELLOW,
    ALL_RED_BEFORE_NORTH,     // degajarea intersectiei
    NORTH_GREEN,
    NORTH_YELLOW,
    ALL_RED_BEFORE_EAST,
    NIGHT_FLASH               // mod noapte: galben intermitent pe ambele
};
constexpr uint8_t TRAFFIC_STATE_COUNT = 8;

enum TrafficEvent : uint8_t {
    EVENT_NORTH_REQUEST = 0,  // buton pietonal / senzor de prezenta pe Nord
    EVENT_PHASE_TIMEOUT,      // faza curenta si-a consumat durata
    EVENT_NIGHT_TOGGLE        // comutator manual de mod noapte
};
constexpr uint8_t TRAFFIC_EVENT_COUNT = 3;

enum LampColor : uint8_t {
    LAMP_OFF = 0,
    LAMP_RED,
    LAMP_YELLOW,
    LAMP_GREEN
};

// Ce arata cele doua capete de semafor. blinkHalfPeriodMs != 0 inseamna ca
// culorile clipesc cu semiperioada data (doar modul noapte).
struct SignalPattern {
    LampColor east;
    LampColor north;
    uint16_t blinkHalfPeriodMs;
};

constexpr uint16_t EAST_MIN_GREEN_MS = 4000;
constexpr uint16_t EAST_HOLD_IDLE_MS = 15000;   // fara cereri -> mod noapte
constexpr uint16_t YELLOW_MS = 2000;
constexpr uint16_t ALL_RED_MS = 1000;
constexpr uint16_t NORTH_GREEN_MS = 5000;
constexpr uint16_t NIGHT_FLASH_PERIOD_MS = 1000;

TrafficState trafficFsmNext(TrafficState current, TrafficEvent event);
SignalPattern trafficFsmPattern(TrafficState state);

// Durata fazei. 0 = starea nu expira singura (modul noapte asteapta un
// eveniment).
uint16_t trafficFsmPhaseMs(TrafficState state);

// true doar in starea care poate ceda verdele directiei Nord chiar acum. In
// celelalte stari cererea se memoreaza si se serveste cand automatul ajunge
// aici, deci apasarile nu se pierd si nici nu scurteaza verdele minim.
bool trafficFsmServesRequest(TrafficState state);

const char *trafficFsmStateName(TrafficState state);
const char *trafficFsmEventName(TrafficEvent event);
const char *lampColorName(LampColor color);

#endif
